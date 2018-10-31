/*
 * tnc_connector.c
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#include "modem.h"

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include <libubox/uloop.h>
#include "xstream.h"

#include "utils.h"
#include "serial.h"
#include "slre.h"

#include "kiss.h"
#include "ax25.h"

#include "config.h"

Modem modem; // shared device instance

typedef enum{
	state_close = 0,
	state_open,
	state_init_request,
	state_ready,
	state_closing, // mark for closing
}modem_state;

typedef enum{
	mode_config = 0,
	mode_kiss,
	mode_tracker,
	mode_repeater
}modem_mode;

static int modem_fd;
static modem_state state;
static modem_mode mode;

static struct uloop_timeout timer;

static struct xstream stream;

static time_t last_reopen = 0, last_init_req = 0, last_keepalive = 0;

#define MAX_INIT_CMDS 8
#define MAX_INIT_CMD_LEN 128
static char initCmds[MAX_INIT_CMDS][MAX_INIT_CMD_LEN];

#define MAX_INIT_ERROR_COUNT 3
#define MAX_READ_WRITE_ERROR_COUNT 10
static int tncErrorCount = 0;

static int modem_open();
static int modem_close();
static int modem_run();

static void tnc_switch_mode(modem_mode mode);
//static int tnc_receiving(int fd);
static void tnc_received(uint8_t* data, size_t len);
static int tnc_send(const char* data,size_t len);
static int tnc_send_init_cmds();
static int tnc_parse_device_info(uint8_t* data, size_t len);


static void timeout_handler(struct uloop_timeout *t){
	modem_run();
	uloop_timeout_set(&timer,5);
}

static void on_stream_read(struct xstream *x, char *data, int len){
	tnc_received((uint8_t*)data, len);
}

static void on_stream_write(struct xstream *x, int len){
	DBG("%d bytes written, %d left", len, x->stream_fd.stream.w.data_bytes);
}

static void on_stream_error(struct xstream *x){
	modem_close();
}

/**
 * Open serial ports
 */
static int modem_open(){
	if(state == state_open) return 0;

	last_reopen = time(NULL);

	//open the tnc serial port - See http://tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html#AEN144
	modem_fd = serial_open(modem.path, modem.speed);
	if(modem_fd < 0){
		return -1;
	}
	
	// setup modem stream
	xstream_crlf_init(&stream,modem_fd, 0, on_stream_read, on_stream_write, on_stream_error);

	state = state_open;
	tncErrorCount = 0;
	last_keepalive = 0;

// #if 1
// 	io_init_stream_reader(&reader,modem_fd,read_buffer,MAX_READ_BUFFER_LEN,350/*read timeout set to 350ms*/,tnc_received);
// #else
// 	io_init_line_reader(&reader,modem_fd,read_buffer,MAX_READ_BUFFER_LEN,tnc_received);
// #endif
	INFO("modem \'%s\' opened, baudrate=%d, fd=%d",modem.path, modem.speed, modem_fd);

	// initialize
	tnc_send_init_cmds();

	// set unblock and select
	set_nonblock(modem_fd,true);
	//io_add(modem_fd,tnc_poll_callback);
	return 0;
}

static int modem_close(){
	if(state == state_close)
		return 0;

	if(modem_fd > 0){ // TODO - move to reader->fnClose();

		xstream_free(&stream);

		//io_remove(modem_fd);
		close(modem_fd);
		modem_fd = -1;
		//reader.fd = -1;
	}
	//reader.fnClose(&reader);
	state = state_close;
	INFO("modem \'%s\' closed.", modem.path);
	return 0;
}

// static int tnc_receiving(int fd){
// 	if(fd != modem_fd) return -1;
// 	int rc = reader.fnRead(&reader);
// 	if(rc < 0){
// 		if(++tncErrorCount >= MAX_READ_WRITE_ERROR_COUNT){
// 			INFO("too much receiving error encountered %d, closing port...",tncErrorCount);
// 			modem_close();
// 		}
// 		return -1;
// 	}else{
// 		tncErrorCount = 0;
// 	}
// 	return 0;
// }

static void tnc_received(uint8_t* data, size_t len){
	//receive from TNC and parse frames
	switch(state){
	case state_init_request:
		// check the init request response
		//tier2_client_send(LOGIN_CMD,strlen(LOGIN_CMD)); // send login command
		DBG("%d bytes received",len);
		if(tnc_parse_device_info(data, len) > 0){
			state = state_ready;
			INFO("tnc initialized.");

			//TODO - switch to kiss mode according to the config
			tnc_switch_mode(mode_kiss);
		}else{
			INFO("tnc initialize failed, got unexpected response.");
			#ifdef DEBUG
			stringdump(data,len);
			#endif
#if 0
			// directly close the tnc port here may cause deadlock here
			modem_close();
			last_reopen = time(NULL); // force to wait REOPEN_WAITTIME seconds to reopen
#endif
		}
		break;
	case state_ready:
		//decode to ax25 message
		DBG("%d bytes received",len);
		AX25Msg msg;
		if(ax25_decode(data,len,&msg) < 0){
			INFO("Unsupported frame, %d bytes",len);
			log_hexdump(data,len);
		}else{
			if(modem.ax25_callback){
				modem.ax25_callback(&msg);
			}
		}

		break;
	default:
		break;
	}

	return ;
}

static int tnc_send(const char* cmd,size_t len){
	int bytesSent = 0;
	if(modem_fd < 0){
		return -1;
	}

	bytesSent = ustream_write(&stream.stream_fd.stream, cmd, len, false);
	if (bytesSent < 0) {
		// something wrong
		ERROR("*** send(): %s.", strerror(errno));
	} else if (bytesSent < len){
		INFO("tnc_send(): %d bytes send, %d left", bytesSent, len - bytesSent);
	}
	return bytesSent;
}

static int tnc_send_init_cmds(){
	//TODO - send initialize command according to the model
	//AT+INFO, AT+CALL, AT+TEXT...
	//tnc_send("AT+KISS=1\n",10);
	INFO("modem initializing...");
	char *cmd = "?\n";
	if(tnc_send(cmd,strlen(cmd)) > 0){
		state = state_init_request;
	}else{
		ERROR("*** tnc_send_init_cmds() failed");
	}
	last_init_req = time(NULL);
	return 0;
}

// static int tnc_send_flush(int fd){
// 	int bytes_send = 0;
// 	char _buf[128];
// 	bzero(_buf,128);
// 	int _buflen = 0;
// 	// pop the cached data
// 	while(!fifo_isempty(&fifoWriteBuffer) && _buflen < 128){
// 		unsigned char c = fifo_pop(&fifoWriteBuffer);
// 		_buf[_buflen++] = c;
// 		/*
// 		if((rc = write(modem_fd, &c,1)) > 0){
// 			len += rc;
// 		}else{
// 			// something wrong
// 			if(errno == EAGAIN || errno == EINVAL){
// 				INFO("!!! write() failed due to non-blocked fd is not ready: %s",strerror(errno));
// 			}else{
// 				ERROR("*** write(): %s.", strerror(errno));
// 			}
// 			break;
// 		}
// 		*/
// 	}
// 	if(_buflen > 0){
// 		//DBG("flushing %s",_buf);
// 		DBG("flushing data: ");
// 		stringdump(_buf,_buflen);
// 		bytes_send = write(modem_fd,_buf,_buflen);
// 		if(bytes_send <=0){
// 			if(errno == EAGAIN || errno == EINVAL){
// 				INFO("!!! write() failed due to non-blocked fd is not ready: %s",strerror(errno));
// 			}else{
// 				ERROR("*** write(): %s.", strerror(errno));
// 			}
// 		}else{
// 			tcdrain(modem_fd);
// 			DBG("flushed write buffer %d of %d bytes.",bytes_send,_buflen);
// 		}
// 	}
// 	return 0;
// }

/*
 * parse "TinyAPRS (KISS-TNC/GPS-Beacon) 1.1-SNAPSHOT (f1a0-3733)"
 * into tnc device info
 */
static int tnc_parse_device_info(uint8_t* data, size_t len){
	struct slre_cap caps[3];
	int ret = -1;
	char * regexp = "TinyAPRS\\s+\\(([a-zA-Z0-9//\\//-]+)\\)\\s+([0-9]+.[0-9]+-[a-zA-Z]+)\\s+\\(([a-zA-Z0-9\\-]+)\\)";
	char * regexp2 ="Mode: \\s([0-9]+)";

	// match device type/versions
	if(len > 20){
		if(slre_match(regexp,(const char*)data,len,caps,3,0) > 0){
			DBG("Found TinyAPRS %.*s, ver: %.*s, rev: %.*s",caps[0].len,caps[0].ptr,caps[1].len,caps[1].ptr,caps[2].len,caps[2].ptr);
			memcpy(modem.model,caps[0].ptr,caps[0].len);
			memcpy(modem.firmware_rev,caps[1].ptr,caps[1].len);
			memcpy(modem.hardware_rev,caps[2].ptr,caps[2].len);
			ret = 1; // detected
		}
	}
	// match device mode
	if(slre_match(regexp2,(const char*)data,len,caps,1,0) > 0 && caps[0].len == 1){
		switch(*(caps[0].ptr)){
		case '0':
			mode = mode_config;
			break;
		case '1':
			mode = mode_kiss;
			break;
		case '2':
			mode = mode_tracker;
			break;
		case '3':
			mode = mode_repeater;
			break;
		default:
			mode = mode_config;
			break;
		}
	}
	DBG("device mode: %d",mode);

	// match device stat

	return ret;
}

static void tnc_switch_mode(modem_mode mode){
	if(mode == mode_kiss){
		//TODO - switch decoder to CRLF or KISS 
		//xstream_set_decode_func(&stream,(xstream_decode_func)kiss_decoder);
	}else{
		//xstream_set_decode_func(&stream,(xstream_decode_func)crlf_decoder);
	}
}

int modem_init(const char* _devname, int32_t _baudrate, const char* _model, char** _initCmds , modem_ax25_decode_callback cb){
	bzero(&modem,sizeof(modem));

	// copy the parameters
	strncpy(modem.path,_devname,31);
	strncpy(modem.model,_model,15);
	modem.speed = _baudrate;

	int i = 0;
	for(i = 0;i<MAX_INIT_CMDS && _initCmds != 0 && *(_initCmds + i) != 0;i++){
		strncpy(initCmds[i],*(_initCmds + i),MAX_INIT_CMD_LEN);
	}
	modem.ax25_callback = cb;

	mode = mode_config;

	// setup uloop_timer
	timer.cb = timeout_handler;
	uloop_timeout_set(&timer,5);

	// Open the device
	modem_open();
	return 0; // always returns success
}

static int modem_run(){
	time_t t = time(NULL);

	switch(state){
	case state_close:
		// re-connect
		if(t - last_reopen > config.tnc[0].current_reopen_wait_time){
			config.tnc[0].current_reopen_wait_time += 30; // increase the reopen wait wait time
			DBG("reopening TNC port...");
			modem_open();
		}
		break;
//	case state_open:
//		tnc_send_init_cmds();
//		break;
	case state_init_request:
		// initialize timeout check
		if(t - last_init_req > config.tnc[0].init_wait_time){
			INFO("modem initialize timeout");
#if 1
			modem_close();
			last_reopen = time(NULL); // force to wait REOPEN_WAITTIME seconds to reopen
#else
			state = state_ready;
			tnc_switch_mode(mode_kiss);
#endif
		}
		break;
	case state_ready:
		if(config.tnc[0].current_reopen_wait_time != config.tnc[0].reopen_wait_time){
			// reset the current wait time, it may be increased during previous failures.
			DBG("Reset current_reopen_wait_time");
			config.tnc[0].current_reopen_wait_time = config.tnc[0].reopen_wait_time;
		}
		if(config.tnc[0].keepalive_wait_time > 0){
			// perform the keepalive
			if(last_keepalive == 0){
				last_keepalive = t;
			}
			if(t - last_keepalive > config.tnc[0].keepalive_wait_time){
				// perform keep_alive ping
				tnc_send("AT+CALL=\n",9);
				last_keepalive = t;
			}
		}
		break;
	default:
		break;
	}

	// io reader runloop;
	// if(reader.fnRun)
	// 	reader.fnRun(&reader);

	return 0;
}
