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


static int tncfd;
static struct termios oldtio,newtio;

static int tnc_open(const char* devName, unsigned int baudrate);
static int tnc_close();
static int tnc_receive(int fd);
static int tnc_send(const char* data,size_t len);


static void tnc_poll_callback(int fd, poll_state state){
	switch(state){
		case poll_state_read:
			tnc_receive(fd);
			break;
		case poll_state_write:
			//TODO -flush the send buffer queue
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
	return 0;
}

static int tnc_close(){
	if(tncfd){
		close(tncfd);
		tncfd = -1;
	}
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
			WARN("!!! write() failed due to non-blocked fd is not ready: %s",strerror(errno));
		}else{
			ERROR("*** write(): %s.", strerror(errno));
		}
	}else if(rc < len){
		WARN("!!! write() %d bytes less than actuall bytes %d.",rc,len);
	}
	return 0;
}


int tnc_init(const char* devName, unsigned int baudrate, const char* model, char** initCmds){
	int rc;
	//open device
	if(devName){
		rc = tnc_open(devName,baudrate);
		if(rc < 0){
			return rc;
		}
	}else{
		tncfd = stdin;
	}

	//send initialize command according to the model


	return 0;
}

int tnc_run(){
	//TODO - read
	return 0;
}
