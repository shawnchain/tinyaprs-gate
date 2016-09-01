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

typedef enum{
	state_close = 0,
	state_open,
	state_init_request,
	state_ready
	//state_reading
}tnc_state;

static int tncfd;
static tnc_state state;
static bool receiving = false;

#define REOPEN_WAIT_TIME 15
#define INIT_RESPONSE_WAIT_TIME 15
#define READ_WAIT_TIME_MS 350.f
static time_t last_reopen = 0,last_init_req = 0;
static double last_read = 0.f;

#define MAX_WRITE_BUFFER_LEN 512
static unsigned char write_buffer[MAX_WRITE_BUFFER_LEN];
static FIFOBuffer fifoWriteBuffer;

#define MAX_INIT_CMDS 8
#define MAX_INIT_CMD_LEN 128
static char devName[64], model[64];
static unsigned int baudrate;
static char initCmds[MAX_INIT_CMDS][MAX_INIT_CMD_LEN];

static int init_error_retry_count = 0;
static int MAX_INIT_RETRY_COUNT = 3;

static int tnc_open();
static int tnc_close();
static int tnc_receiving(int fd);
static int tnc_received(char* data, size_t len);
static int tnc_send(const char* data,size_t len);
static int tnc_send_init_cmds();
static int tnc_send_flush(int fd);
static bool tnc_can_write();

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

static struct termios oldtio,newtio;
static int tnc_open(){
	if(state == state_open) return 0;

	last_reopen = time(NULL);
	//open the tnc serial port - See http://tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html#AEN144
	tncfd = open(devName, O_RDWR | O_NOCTTY |/*O_NONBLOCK |*/ O_NDELAY);
	if(tncfd < 0){
		ERROR("*** open() tnc port \"%s\" failed: %s.", devName, strerror(errno));
		return -1;
	}
	fcntl(tncfd,F_SETOWN,getpid());
	//fcntl(tncfd, F_SETFL, 0); // clear all flags
	//fcntl(tncfd, F_SETFL, FNDELAY); // set read nonblocking

	DBG("read old serial config");
	bool hardflow = false;
	tcgetattr(tncfd,&oldtio); /* save current port settings */
	bzero(&newtio, sizeof(newtio));
	memcpy(&newtio,&oldtio,sizeof(newtio));
#if 1 //Non Canonical Input Processing
	newtio.c_iflag &= ~(IMAXBEL|INPCK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IGNPAR);
	newtio.c_iflag &= ~(IXON | IXOFF | IXANY); // disable software flow control
	newtio.c_iflag |= IGNBRK;

	newtio.c_oflag &= ~OPOST; // unset the OPOST for raw output
	//newtio.c_oflag |= (OPOST|ONLCR);

	if (hardflow) {
		newtio.c_cflag |= CRTSCTS;
	} else {
		newtio.c_cflag &= ~CRTSCTS;
	}

	newtio.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|ISIG|IEXTEN|NOFLSH|TOSTOP|PENDIN);
	newtio.c_cflag &= ~(CSIZE|PARENB|CSTOPB); // 8N1
	newtio.c_cflag |= (CS8|CREAD|CLOCAL);
	newtio.c_cc[VMIN] = 0;//80;
	newtio.c_cc[VTIME] = 3;//3;

//	newtio.c_iflag = IGNPAR;
//	newtio.c_oflag = 0;
//	/* set input mode (non-canonical, no echo,...) */
//	newtio.c_lflag = 0;
//	newtio.c_cc[VTIME]    = 3;   	/* inter-character timer unused */
//	newtio.c_cc[VMIN]     = 80;   	/* blocking read until 5 chars received */

	// setup the baud rate
	cfsetispeed(&newtio, baudrate);
	cfsetospeed(&newtio, baudrate);

#else //Canonical Input Processing
	/* set new port settings for canonical input processing */
	newtio.c_cflag = baudrate | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | INLCR;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;

//	newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */
//	newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
//	newtio.c_cc[VERASE]   = 0;     /* del */
//	newtio.c_cc[VKILL]    = 0;     /* @ */
//	newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
	newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
//	newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */
//	newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
//	newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
//	newtio.c_cc[VEOL]     = 0;     /* '\0' */
//	newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
//	newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
//	newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
//	newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
//	newtio.c_cc[VEOL2]    = 0;     /* '\0' */
#endif
	tcflush(tncfd, TCIOFLUSH);
	DBG("set new serial config");
	if(tcsetattr(tncfd,TCSANOW,&newtio) < 0){
		ERROR("*** tcsetattr() failed: %s.",strerror(errno));
		close(tncfd);
		tncfd = -1;
		return -1;
	}

	state = state_open;
	init_error_retry_count = 0;
	INFO("tnc open \"%s\" success",devName);

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
	INFO("tnc_close() ok");
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
		ERROR("*** tnc_receiving() error: %s",strerror(errno));
		return -1;
	}
	read_buffer_len += bytesRead;
	if(!receiving){
		receiving = true;
		DBG("Receiving data");
	}

	last_read = get_time_milli_seconds(); // wait for the read timeout to flush the buffer
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
		state = state_ready;
		//tier2_client_send(LOGIN_CMD,strlen(LOGIN_CMD)); // send login command
		DBG("%d bytes received",len);
		dump(data,len);
		INFO("tnc initialized OK");
		break;
	case state_ready:
		//TODO - parse the tnc received frame
		//state = state_server_logged_in;
		DBG("%d bytes received",len);
		dump(data,len);
		//hexdump(data,len);
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
		tcflush(tncfd, TCIOFLUSH);
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
#if 0
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
		dump(_buf,_buflen);
		bytes_send = write(tncfd,_buf,_buflen);
		if(bytes_send <0){
			if(errno == EAGAIN || errno == EINVAL){
				INFO("!!! write() failed due to non-blocked fd is not ready: %s",strerror(errno));
			}else{
				ERROR("*** write(): %s.", strerror(errno));
			}
		}else{
			tcflush(tncfd, TCIOFLUSH);
			DBG("flushed write buffer %d of %d bytes.",bytes_send,_buflen);
		}
	}

	return 0;
}

int tnc_init(const char* _devName, unsigned int _baudrate, const char* _model, char** _initCmds){
	// copy the parameters
	strncpy(devName,_devName,63);
	strncpy(model,_model,63);
	baudrate = _baudrate;
	for(int i = 0;i<MAX_INIT_CMDS && _initCmds != 0 && *(_initCmds + i) != 0;i++){
		strncpy(initCmds[i],*(_initCmds + i),MAX_INIT_CMD_LEN);
	}
	// initialize the write buffer
	fifo_init(&fifoWriteBuffer,write_buffer,MAX_WRITE_BUFFER_LEN);

	tnc_open();
	return 0; // always returns success
}

int tnc_run(){
	time_t t = time(NULL);
	if(state == state_close){
		// try to reconnect
		if(t - last_reopen > REOPEN_WAIT_TIME){
			DBG("reopening tnc port...");
			tnc_open();
		}
	}else if(state == state_open){
		// try to send the init string
		INFO("tnc initializing...");
		last_init_req = time(NULL);
		tnc_send_init_cmds();
		state = state_init_request;
	}else if(state == state_init_request){
		// check timeout
		if(t - last_init_req > INIT_RESPONSE_WAIT_TIME){
			INFO("!!! tnc initializing failed, wait for response timeout");
			if(++init_error_retry_count >= MAX_INIT_RETRY_COUNT){
				tnc_close();
				last_reopen = time(NULL); // force to wait REOPEN_WAITTIME seconds to reopen
			}else{
				state = state_open;
			}
		}
	}

	if(receiving && read_buffer_len > 0){
		// timeout check
		double c = get_time_milli_seconds();
		if(c - last_read > READ_WAIT_TIME_MS /*ms*/){
			// read timeout
			receiving = false;
			tnc_received(read_buffer,read_buffer_len);
			read_buffer_len = 0;
		}
	}
	return 0;
}
