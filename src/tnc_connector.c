/*
 * tnc_connector.c
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#include "tnc_connector.h"

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

#include <strings.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include "tier2_client.h"
#include "utils.h"
#include "fifobuf.h"
#include "serial_port.h"
#include "slre.h"

#include "kiss.h"
#include "ax25.h"

#include "config.h"

TNC tnc; // shared device instance

typedef enum{
	state_close = 0,
	state_open,
	state_init_request,
	state_ready,
	state_closing, // mark for closing
}tnc_state;

typedef enum{
	mode_config = 0,
	mode_kiss,
	mode_tracker,
	mode_repeater
}tnc_mode;

static int tncfd;
static tnc_state state;
static tnc_mode mode;

static tnc_ax25_decode_callback client_message_callback;

static time_t last_reopen = 0, last_init_req = 0, last_keepalive = 0;

#define MAX_READ_BUFFER_LEN 2048
static uint8_t read_buffer[MAX_READ_BUFFER_LEN];
#define MAX_WRITE_BUFFER_LEN 512
static uint8_t write_buffer[MAX_WRITE_BUFFER_LEN];
static struct IOReader reader;
static FIFOBuffer fifoWriteBuffer;

#define MAX_INIT_CMDS 8
#define MAX_INIT_CMD_LEN 128
static char initCmds[MAX_INIT_CMDS][MAX_INIT_CMD_LEN];

#define MAX_INIT_ERROR_COUNT 3
#define MAX_READ_WRITE_ERROR_COUNT 10
static int tncErrorCount = 0;

static int tnc_open();
static int tnc_close();
static void tnc_switch_mode(tnc_mode mode);
static int tnc_receiving(int fd);
static void tnc_received(uint8_t* data, size_t len);
static int tnc_send(const char* data,size_t len);
static int tnc_send_init_cmds();
static int tnc_send_flush(int fd);
static bool tnc_can_write();
static int tnc_parse_device_info(uint8_t* data, size_t len);

static void tnc_poll_callback(int fd, poll_state state){
	switch(state){
		case poll_state_read:
			tnc_receiving(fd);
			break;
		case poll_state_write:
			//flush the send buffer queue
			tnc_send_flush(fd);
			break;
		case poll_state_error:
			INFO("Polled error, closing port...");
			tnc_close();
			break;
		case poll_state_idle:
			//DBG("tnc idle");
			break;
		default:
			break;
	}
}

/**
 * Open serial ports
 */
static int tnc_open(){
	if(state == state_open) return 0;

	last_reopen = time(NULL);

	//open the tnc serial port - See http://tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html#AEN144
	tncfd = serial_port_open(tnc.devname, tnc.baudrate);
	if(tncfd < 0){
		return -1;
	}

	state = state_open;
	tncErrorCount = 0;
	last_keepalive = 0;
#if 1
	io_init_timeoutreader(&reader,tncfd,read_buffer,MAX_READ_BUFFER_LEN,350/*read timeout set to 350ms*/,tnc_received);
#else
	io_init_linereader(&reader,tncfd,read_buffer,MAX_READ_BUFFER_LEN,tnc_received);
#endif
	INFO("tnc port opened \"%s\", fd is %d",tnc.devname,tncfd);

	// initialize
	tnc_send_init_cmds();

	// set unblock and select
	serial_port_set_nonblock(tncfd,1);
	poll_add(tncfd,tnc_poll_callback);
	return 0;
}

static int tnc_close(){
	if(state == state_close) return 0;

	if(tncfd > 0){ // TODO - move to reader->fnClose();
		poll_remove(tncfd);
		close(tncfd);
		tncfd = -1;
		reader.fd = -1;
	}
	reader.fnClose(&reader);
	state = state_close;
	INFO("tnc port closed.");
	return 0;
}

static int tnc_receiving(int fd){
	if(fd != tncfd) return -1;
	int rc = reader.fnRead(&reader);
	if(rc <=0){
		if(++tncErrorCount >= MAX_READ_WRITE_ERROR_COUNT){
			INFO("too much receiving error encountered %d, closing port...",tncErrorCount);
			tnc_close();
		}
		return -1;
	}else{
		tncErrorCount = 0;
	}
	return 0;
}

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
			tnc_close();
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
			if(client_message_callback){
				client_message_callback(&msg);
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
	if(tncfd < 0){
		return -1;
	}

	if(tnc_can_write()){
		bytesSent = write(tncfd,cmd,len);
	}
	if(bytesSent < 0){
		// something wrong
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			//INFO("!!! write() failed due to non-blocked fd is not ready: %s",strerror(errno));
		}else{
			ERROR("*** write(): %s.", strerror(errno));
		}
		bytesSent = 0;
	}

	if(bytesSent < len){
		int i = 0;
		for(i = bytesSent;i<len;i++){
			if(!fifo_isfull(&fifoWriteBuffer)){
				fifo_push(&fifoWriteBuffer,cmd[i]);
			}else{
				WARN("*** write buffer is full, %d bytes dropped",len - i);
				break;
			}
		}
		DBG("%d bytes buffered",len - bytesSent);
	}else{
		DBG("%d of %d bytes sent",bytesSent,len);
	}
	if(bytesSent > 0){
		tcdrain(tncfd);
	}
	return bytesSent;
}

static int tnc_send_init_cmds(){
	//TODO - send initialize command according to the model
	//AT+INFO, AT+CALL, AT+TEXT...
	//tnc_send("AT+KISS=1\n",10);
	INFO("tnc initializing...");
	char *cmd = "?\n";
	if(tnc_send(cmd,strlen(cmd)) > 0){
		state = state_init_request;
	}else{
		ERROR("*** tnc_send_init_cmds() failed");
	}
	last_init_req = time(NULL);
	return 0;
}

static bool tnc_can_write(){
#if 1
	fd_set wset;
	struct timeval timeo;
	int rc;

	FD_ZERO(&wset);
	FD_SET(tncfd, &wset);
	timeo.tv_sec = 0;
	timeo.tv_usec = 0;
	rc = select(tncfd + 1, NULL, &wset, NULL, &timeo);
	if(rc >0 && FD_ISSET(tncfd, &wset)){
		return true;
	}
#endif
	return false;
}

static int tnc_send_flush(int fd){
	int bytes_send = 0;
	char _buf[128];
	bzero(_buf,128);
	int _buflen = 0;
	// pop the cached data
	while(!fifo_isempty(&fifoWriteBuffer) && _buflen < 128){
		unsigned char c = fifo_pop(&fifoWriteBuffer);
		_buf[_buflen++] = c;
		/*
		if((rc = write(tncfd, &c,1)) > 0){
			len += rc;
		}else{
			// something wrong
			if(errno == EAGAIN || errno == EINVAL){
				INFO("!!! write() failed due to non-blocked fd is not ready: %s",strerror(errno));
			}else{
				ERROR("*** write(): %s.", strerror(errno));
			}
			break;
		}
		*/
	}
	if(_buflen > 0){
		//DBG("flushing %s",_buf);
		DBG("flushing data: ");
		stringdump(_buf,_buflen);
		bytes_send = write(tncfd,_buf,_buflen);
		if(bytes_send <=0){
			if(errno == EAGAIN || errno == EINVAL){
				INFO("!!! write() failed due to non-blocked fd is not ready: %s",strerror(errno));
			}else{
				ERROR("*** write(): %s.", strerror(errno));
			}
		}else{
			tcdrain(tncfd);
			DBG("flushed write buffer %d of %d bytes.",bytes_send,_buflen);
		}
	}
	return 0;
}

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
			memcpy(tnc.model,caps[0].ptr,caps[0].len);
			memcpy(tnc.firmware_rev,caps[1].ptr,caps[1].len);
			memcpy(tnc.board_rev,caps[2].ptr,caps[2].len);
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

static void tnc_switch_mode(tnc_mode mode){
	if(mode == mode_kiss){
#if 1
		kiss_init(&reader,tncfd,read_buffer,MAX_READ_BUFFER_LEN, tnc_received);
#else
		io_init_timeoutreader(&reader,tncfd,read_buffer,MAX_READ_BUFFER_LEN,350/*read timeout set to 350ms*/,tnc_received);
#endif
	}else{

	}
}

int tnc_init(const char* _devname, int _baudrate, const char* _model, char** _initCmds , tnc_ax25_decode_callback cb){
	bzero(&tnc,sizeof(tnc));

	// copy the parameters
	strncpy(tnc.devname,_devname,31);
	strncpy(tnc.model,_model,15);
	tnc.baudrate = _baudrate;
	int i = 0;
	for(i = 0;i<MAX_INIT_CMDS && _initCmds != 0 && *(_initCmds + i) != 0;i++){
		strncpy(initCmds[i],*(_initCmds + i),MAX_INIT_CMD_LEN);
	}
	client_message_callback = cb;

	mode = mode_config;

	// initialize the write buffer
	fifo_init(&fifoWriteBuffer,write_buffer,MAX_WRITE_BUFFER_LEN);

	// Open the device
	tnc_open();
	return 0; // always returns success
}

int tnc_run(){
	time_t t = time(NULL);

	switch(state){
	case state_close:
		// re-connect
		if(t - last_reopen > config.tnc[0].current_reopen_wait_time){
			config.tnc[0].current_reopen_wait_time += 30; // increase the reopen wait wait time
			DBG("reopening TNC port...");
			tnc_open();
		}
		break;
//	case state_open:
//		tnc_send_init_cmds();
//		break;
	case state_init_request:
		// initialize timeout check
		if(t - last_init_req > config.tnc[0].init_wait_time){
			INFO("tnc initialize timeout");
#if 0
			tnc_close();
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
	if(reader.fnRun)
		reader.fnRun(&reader);

	return 0;
}
