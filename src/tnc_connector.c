/*
 * tnc_connector.c
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

#include "tier2_client.h"
#include "utils.h"
#include "fifobuf.h"
#include "serial_port.h"
#include "slre.h"

#include "tnc_connector.h"

TNC tnc; // shared device instance

static TNCConfig tnc_config = {
	.reopen_wait_time = 15,
	.init_wait_time = 15,
	.read_wait_time_ms = 350,
	.keepalive_wait_time = -1,
};

typedef enum{
	state_close = 0,
	state_open,
	state_init_request,
	state_ready
}tnc_state;

static int tncfd;
static tnc_state state;
static bool receiving = false;

static tnc_packet_callback packetcb;

static time_t last_reopen = 0, last_init_req = 0, last_keepalive = 0,last_read_ms = 0;

#define MAX_WRITE_BUFFER_LEN 512
static unsigned char write_buffer[MAX_WRITE_BUFFER_LEN];
static FIFOBuffer fifoWriteBuffer;

#define MAX_INIT_CMDS 8
#define MAX_INIT_CMD_LEN 128
static char initCmds[MAX_INIT_CMDS][MAX_INIT_CMD_LEN];

#define MAX_INIT_ERROR_COUNT 3
static int init_error_count = 0;
#define MAX_READ_WRITE_ERROR_COUNT 10
static int read_write_error_count = 0;

static int tnc_open();
static int tnc_close();
static int tnc_receiving(int fd);
static int tnc_received(char* data, size_t len);
static int tnc_send(const char* data,size_t len);
static int tnc_send_init_cmds();
static int tnc_send_flush(int fd);
static bool tnc_can_write();
static int tnc_parse_device_info(char* data, size_t len);

static void tnc_poll_callback(int fd, poll_state state){
	switch(state){
		case poll_state_read:
			tnc_receiving(fd);
			break;
		case poll_state_write:
			//TODO -flush the send buffer queue
			tnc_send_flush(fd);
			break;
		case poll_state_error:
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
	init_error_count = 0;
	read_write_error_count = 0;
	last_keepalive = 0;
	receiving = false;
	INFO("open \"%s\" success",tnc.devname);

	// initialize
	INFO("tnc initializing...");
	const char* initcmd = "?\n";
	int initcmd_len = strlen(initcmd);
	int wlen = write(tncfd,initcmd,initcmd_len);
	if(wlen != 2){
		WARN("write %d of %d bytes init command",wlen,initcmd_len);
	}
	tcdrain(tncfd); // wait for the serial port write complete
	state = state_init_request;
	last_init_req = time(NULL);
#if 0
    do {
        unsigned char buf[80];
        int rdlen;

        rdlen = read(tncfd, buf, sizeof(buf) - 1);
        if (rdlen > 0) {
            buf[rdlen] = 0;
            printf("Read %d: \"%s\"\n", rdlen, buf);
        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
        }
        /* repeat read to get full message */
    } while (1);
#endif

	// set unblock and select
	serial_port_set_nonblock(tncfd,1);
	poll_add(tncfd,tnc_poll_callback);
	return 0;
}

static int tnc_close(){
	if(state == state_close) return 0;

	if(tncfd){
		poll_remove(tncfd);
		close(tncfd);
		tncfd = -1;
	}
	state = state_close;
	INFO("tnc closed.");
	return 0;
}

#define MIN(X,Y) (X<Y?X:Y)
#define MAX_READ_BUFFER_LEN 2048
static char read_buffer[MAX_READ_BUFFER_LEN];
static int read_buffer_len = 0;

static int tnc_receiving(int fd){
	int bytes_available = MAX_READ_BUFFER_LEN - read_buffer_len;
	int bytesRead = 0;

	bytesRead = read(fd,read_buffer + read_buffer_len,bytes_available);
	if(bytesRead <= 0){
		ERROR("*** tnc_receiving %d, read() error: %s",bytesRead, strerror(errno));
		if(++read_write_error_count >= MAX_READ_WRITE_ERROR_COUNT){
			tnc_close();
		}
		return -1;
	}
	read_buffer_len += bytesRead;
	if(!receiving){
		receiving = true;
		DBG("Receiving data");
	}

	last_read_ms = get_time_milli_seconds(); // wait for the read timeout to flush the buffer
	if(read_buffer_len == MAX_READ_BUFFER_LEN){
		tnc_received(read_buffer,MAX_READ_BUFFER_LEN);
		read_buffer_len = 0;// reset the buffer
		receiving = false;
	}

	return 0;
}

static int tnc_received(char* data, size_t len){
	//receive from TNC and parse frames
	switch(state){
	case state_init_request:
		// check the init request response
#ifdef DEBUG
		stringdump(data,len);
#endif
		//tier2_client_send(LOGIN_CMD,strlen(LOGIN_CMD)); // send login command
		DBG("%d bytes received",len);

		if(tnc_parse_device_info(data, len) > 0){
			state = state_ready;
			INFO("tnc initialized.");
		}else{
			INFO("Unknow tnc response.");
		}

		break;
	case state_ready:
		//TODO - parse the tnc received frame
		DBG("%d bytes received",len);
		//TODO - decode data
		//if(decode(data,len)...
		if(packetcb){
			packetcb(data,len);
		}
		break;
	//case state_reading:
		// accumulate the received bytes until read timeout
	default:
		break;
	}

	return 0;
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
		if(errno == EAGAIN || errno == EINVAL){
			INFO("!!! write() failed due to non-blocked fd is not ready: %s",strerror(errno));
		}else{
			ERROR("*** write(): %s.", strerror(errno));
		}
		bytesSent = 0;
	}

	if(bytesSent < len){
		for(int i = bytesSent;i<len;i++){
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
		//tcflush(tncfd, TCIOFLUSH);
	}
	return 0;
}

static int tnc_send_init_cmds(){
	//TODO - send initialize command according to the model
	//AT+INFO, AT+CALL, AT+TEXT...
	//tnc_send("AT+KISS=1\n",10);
	char *cmd = "?\n";
	tnc_send(cmd,strlen(cmd));
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
			//tcflush(tncfd, TCIOFLUSH);
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
static int tnc_parse_device_info(char* data, size_t len){
	struct slre_cap caps[3];
	char * regexp = "^TinyAPRS\\s+\\(([a-zA-Z0-9//\\//-]+)\\)\\s+([0-9]+.[0-9]+-[a-zA-Z]+)\\s+\\(([a-zA-Z0-9\\-]+)\\)$";
	char *start=data,*end = data;
	for(int i = 0;i<len;i++){
		if((data[i] == '\r' || data[i] == '\n') && (i > 0)){
			end = data + i;
			if(end - start > 20){
				//DBG("possible string %d bytes ",(end - start));
				// use regexp
				char buf[128];
				bzero(buf,128);
				memcpy(buf,start,(end-start));
				if(slre_match(regexp,buf,strlen(buf),caps,3,0) > 0){
					DBG("Found TinyAPRS %.*s, ver: %.*s, rev: %.*s",caps[0].len,caps[0].ptr,caps[1].len,caps[1].ptr,caps[2].len,caps[2].ptr);
					memcpy(tnc.model,caps[0].ptr,caps[0].len);
					memcpy(tnc.firmware_rev,caps[1].ptr,caps[1].len);
					memcpy(tnc.board_rev,caps[2].ptr,caps[2].len);
					//DBG("-> %.*s, %.*s,",caps[0].len,caps[0].ptr,caps[1].len,caps[1].ptr);
					return 1;
				}else{
					//DBG("-> %s",buf);
				}
			}
			start = end + 1;
		}
	}
	return -1;
}

int tnc_init(const char* _devname, int _baudrate, const char* _model, char** _initCmds , tnc_packet_callback _packetcb){
	bzero(&tnc,sizeof(tnc));

	// copy the parameters
	strncpy(tnc.devname,_devname,31);
	strncpy(tnc.model,_model,15);
	tnc.baudrate = _baudrate;
	for(int i = 0;i<MAX_INIT_CMDS && _initCmds != 0 && *(_initCmds + i) != 0;i++){
		strncpy(initCmds[i],*(_initCmds + i),MAX_INIT_CMD_LEN);
	}
	packetcb = _packetcb;

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
		// try to reconnect
		if(t - last_reopen > tnc_config.reopen_wait_time){
			DBG("reopening tnc port...");
			tnc_open();
		}
		break;
	case state_open:
		// try to send the init string
		INFO("tnc initializing...");
		last_init_req = time(NULL);
		tnc_send_init_cmds();
		state = state_init_request;
		break;
	case state_init_request:
		// check timeout
		if(t - last_init_req > tnc_config.init_wait_time){
			INFO("*** tnc initialize failed, wait for response timeout");
			if(++init_error_count >= MAX_INIT_ERROR_COUNT){
				tnc_close();
				last_reopen = time(NULL); // force to wait REOPEN_WAITTIME seconds to reopen
			}else{
				state = state_open;
			}
		}
		break;
	case state_ready:
		if(tnc_config.keepalive_wait_time > 0){
			// perform the keepalive
			if(last_keepalive == 0){
				last_keepalive = t;
			}
			if(t - last_keepalive > tnc_config.keepalive_wait_time){
				// perform keep_alive ping
				tnc_send("AT+CALL=\n",9);
				last_keepalive = t;
			}
		}
		break;
	default:
		break;
	}

	if(receiving && read_buffer_len > 0){
		// timeout check
		time_t c = get_time_milli_seconds();
		if(c - last_read_ms > tnc_config.read_wait_time_ms){
			// read timeout
			receiving = false;
			tnc_received(read_buffer,read_buffer_len);
			read_buffer_len = 0;
		}
	}
	return 0;
}
