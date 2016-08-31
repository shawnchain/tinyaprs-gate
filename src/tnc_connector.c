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

#include "tier2_client.h"
#include "utils.h"
#include "fifobuf.h"

typedef enum{
	state_close = 0,
	state_open,
	state_wait_for_response
}tnc_state;

static int tncfd;
static tnc_state state;

static struct termios oldtio,newtio;

#define MAX_WRITE_BUFFER_LEN 512
static unsigned char write_buffer[MAX_WRITE_BUFFER_LEN];
static FIFOBuffer fifoWriteBuffer;

static int tnc_open(const char* devName, unsigned int baudrate);
static int tnc_close();
static int tnc_receive(int fd);
static int tnc_send(const char* data,size_t len);
static int tnc_send_flush(int fd);


static void tnc_poll_callback(int fd, poll_state state){
	switch(state){
		case poll_state_read:
			tnc_receive(fd);
			break;
		case poll_state_write:
			//TODO -flush the send buffer queue
			tnc_send_flush(fd);
			break;
		case poll_state_error:
			tnc_close();
			break;
		case poll_state_idle:
			break;
		default:
			break;
	}
}

static int tnc_open(const char* devName, unsigned int baudrate){
	if(state == state_open) return 0;

	//TODO - open the tnc serial port, send the initialize command
	// See http://tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html#AEN144
	tncfd = open(devName, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(tncfd < 0){
		ERROR("*** open() serial port failed: %s.", strerror(errno));
		return -1;
	}

	// setup the baud rate
	tcgetattr(tncfd,&oldtio); /* save current port settings */
	bzero(&newtio, sizeof(newtio));

#if 0 //Canonical Input Processing
	/* set new port settings for canonical input processing */
	newtio.c_cflag = baudrate | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	newtio.c_cc[VMIN]=1;
	newtio.c_cc[VTIME]=0;
#else //Non Canonical Input Processing
	newtio.c_cflag = baudrate | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */
#endif
	tcflush(tncfd, TCIFLUSH);
	tcsetattr(tncfd,TCSANOW,&newtio);

	// add to poll
	poll_add(tncfd,tnc_poll_callback);

	state = state_open;
	return 0;
}

static int tnc_close(){
	if(state == state_close) return 0;

	if(tncfd){
		close(tncfd);
		tncfd = -1;
	}
	state = state_close;
	return 0;
}

static int tnc_receive(int fd){
	//TODO receive from TNC and parse frames
	return 0;
}

static int tnc_send(const char* cmd,size_t len){
	int rc;
	if(tncfd < 0){
		return -1;
	}
	rc = write(tncfd,cmd,len);
	if(rc < 0){
		// something wrong
		if(errno == EAGAIN || errno == EINVAL){
			INFO("!!! write() failed due to non-blocked fd is not ready: %s",strerror(errno));
		}else{
			ERROR("*** write(): %s.", strerror(errno));
		}
		rc = 0;
	}
	if(rc < len){
		for(int i = rc;i<len;i++){
			if(!fifo_isfull(&fifoWriteBuffer)){
				fifo_push(&fifoWriteBuffer,cmd[i]);
			}else{
				WARN("*** write buffer is full, %d bytes dropped",len - i);
				break;
			}
		}
	}
	INFO("%d of %d bytes sent",rc,len);

	return 0;
}

//static bool tnc_can_write(){
//	return false;
//}

static int tnc_send_flush(int fd){
	int rc = 0,len = 0;
	while(!fifo_isempty(&fifoWriteBuffer)){
		unsigned char c = fifo_pop(&fifoWriteBuffer);
		if(write(fd, &c,1) > 0){
			len += rc;
		}else{
			// something wrong
			break;
		}
	}
	if(len > 0){
		DBG("flushed write buffer %d bytes.",len);
	}
	return 0;
}

int tnc_init(const char* devName, unsigned int baudrate, const char* model, char** initCmds){
	int rc;

	// initialize the write buffer
	fifo_init(&fifoWriteBuffer,write_buffer,MAX_WRITE_BUFFER_LEN);

	//open device
	if(devName){
		rc = tnc_open(devName,baudrate);
		if(rc < 0){
			return rc;
		}
	}else{
		tncfd = 0 /*stdin*/;
	}

	//TODO - send initialize command according to the model
	//AT+INFO
	//AT+CALL
	//AT+TEXT
	tnc_send("AT+KISS=1\n",10);

	return 0;
}

int tnc_run(){
	//TODO - read
	return 0;
}
