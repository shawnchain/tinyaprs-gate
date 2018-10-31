/*
 * tier2_client.c
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */

#include "t2_connector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "tinyaprs_gate.h"
#include "utils.h"
#include "slre.h"

#include "config.h"
#include "iokit.h"

#define buffer_len 4 * 1024

typedef enum{
	state_disconnected = 0,
	state_connected,
	state_server_prompt,
	state_server_unverified,
	state_server_verified,
}tier2_client_state;

static tier2_client_state state = 0;
static int sockfd = -1;
static time_t last_reconnect = 0, last_recv = 0, last_keepalive = 0;
static struct sockaddr_inx server_addr;

static int tier2_client_connect();
static int tier2_client_disconnect();
static int tier2_client_receive(int fd);
static void tier2_client_keepalive();

static void tier2_client_poll_callback(int fd, io_state state){
	switch(state){
		case io_state_read:
			tier2_client_receive(fd);
			break;
		case io_state_write:
			//TODO -flush the send buffer queue
			break;
		case io_state_error:
			tier2_client_disconnect();
			break;
		case io_state_idle:
			break;
		default:
			break;
	}
}

static char server[64];

int t2_connector_init(const char* _host){
	// copy the parameters
	strncpy(server,_host,63);

	int rc;
	rc = tier2_client_connect();
	if(rc == -EAGAIN){
		// resolve ok but connect failed, will retry later
		WARN("Connect to '%s' temporarily failed, will re-connect later.", server);
	}else if(rc == -EINVAL){
		ERROR("*** Invalid server address: '%s'.", server);
		return -1;
	}else if(rc < 0){
		ERROR("*** Unable to connect to '%s'.", server);
		return -1;
	}else{
		INFO("Server Connected");
	}

	return 0;
}

#define RECONNECT_WAITTIME 15
#define KEEPALIVE_TIMEOUT 120
#define IDLE_TIMEOUT 90
static int32_t current_reconnect_waittime = RECONNECT_WAITTIME;

int t2_connector_run(){
	time_t t = time(NULL);
	if(state == state_disconnected){
		// try to reconnect
		if(t - last_reconnect > current_reconnect_waittime){
			INFO("reconnecting...");
			current_reconnect_waittime += 15;
			tier2_client_connect();
		}
	}else{
		if(t - last_recv > IDLE_TIMEOUT){
			// try to reconnect as we have 90s idle, as the server will push every 20 seconds
			INFO("IDLE timeout, reconnecting...");
			tier2_client_disconnect();
			tier2_client_connect();
		}
		if(state == state_connected){
			if(current_reconnect_waittime != RECONNECT_WAITTIME){
				DBG("Reset current_reconnect_waittime");
				current_reconnect_waittime = RECONNECT_WAITTIME;
			}
		}
		if(state == state_server_verified || state == state_server_unverified){
			if(t - last_keepalive > KEEPALIVE_TIMEOUT){
				tier2_client_keepalive();
			}
		}
	}
	return 0;
}

static int tier2_client_connect() {
	int rc;
	last_reconnect = time(NULL);
	last_keepalive = last_reconnect;

	if(state != state_disconnected){
		// already connected
		return 0;
	}

	INFO("Connecting to %s", server);

	if ((rc = resolve_host(server, &server_addr)) < 0) {
		// resolve host error, bail out
		WARN("resolve server %s failed, connect aborted.",server);
		return rc;
	}

	last_recv = time(NULL);

	if ((sockfd = socket((&server_addr)->sa.sa_family, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		ERROR("*** socket() failed: %s.", strerror(errno));
		return -1;
	}

	int optval = 1;
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int)) < 0){
	    WARN("*** can not set TCP_NODELAY option on  socket (%s)\n", strerror(errno));
	}

	if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof_sockaddr(&server_addr)) < 0) {
		tier2_client_disconnect();
		return -EAGAIN;
	}
	set_nonblock(sockfd, true);
	io_add(sockfd,tier2_client_poll_callback);

	state = state_connected;

	return 0;
}

static int tier2_client_disconnect(){
	state = state_disconnected;
	if(sockfd >= 0){
		io_remove(sockfd);
		close(sockfd);
		sockfd = -1;
	}
	INFO("tier2_client_disconnect()");
	return 0;
}

/**
 * logresp BG5HHP verified, server T2XWT
 */
static bool tier2_client_verifylogin(const char* resp, size_t len){
	struct slre_cap caps[2];
	char * regexp = "^# logresp ([a-zA-Z0-9//-]+) verified, server ([a-zA-Z0-9]+)";
	if(slre_match(regexp,resp,len,caps,2,0) > 0){
		DBG("%.*s verified, server: %.*s",caps[0].len,caps[0].ptr,caps[1].len,caps[1].ptr);
		//DBG("-> %.*s, %.*s,",caps[0].len,caps[0].ptr,caps[1].len,caps[1].ptr);
		return true;
	}else{
		//DBG("-> %s",buf);
	}
	return false;
}

//const char* LOGIN_CMD = "user TINYIS pass -1 vers TinyAprsGate 0.1 filter r/36.045101/103.836093/1500\r\n";
const char* LOGIN_CMD = "user %s pass %s vers TinyAPRS 0.1 filter %s\r\n";
const char* KEEPALIVE_CMD = "#TinyAprsGate 0.1\r\n";

/*
 * dump server message, which is CRLF ended
 */
static void dump_server_messages(char* data,size_t len){
	int i = 0;
	char* start = data;
	while(data[i] != 0 && i < len){
		if(data[i] == '\r' || data[i] == '\n'){
			data[i] = 0;
			if(data + i - start >1)
				printf(">From IS: %s\n",start);
			start = (data + i + 1);
		}
		i++;
	}
}

/*
 * remove the trailing CRLF for better log
 */
static char* rightTrimCRLF(char *src, size_t len){
#if 1
	bool found = false;
	int i = len;
	while(i >0 ){
		if(src[i] == '\r' || src[i] == '\n'){
			found = true;
			src[i] = 0;
		}else if(found){
			break;
		}
		i--;
	}
#endif
	return src;
}

static int tier2_client_receive(int _sockfd) {
	int bytesRead;
	char read_buffer[buffer_len];
	bzero(&read_buffer,buffer_len);
	bytesRead = recv(_sockfd,&read_buffer, buffer_len,0);
	if(bytesRead <=0){
		// socket closed or something wrong
		return -1;
	}
	last_recv = time(NULL);

	// removing the trailing <CR><LF>
	switch(state){
	case state_connected:
		INFO("Server Greeting: %s",rightTrimCRLF(read_buffer,bytesRead));
		state = state_server_prompt;
		char loginCmd[512];
		int cmdLen = snprintf(loginCmd,511,LOGIN_CMD,config.callsign,config.passcode,config.filter);
		INFO("Login Request: %.*s",cmdLen - 2 /*not logging the trailing CRLF chars*/, loginCmd);
		t2_connector_send(loginCmd,cmdLen); // send login command
		break;
	case state_server_prompt:
		//TODO - check  server login respond
		INFO("Server Respond: %s",rightTrimCRLF(read_buffer,bytesRead));
		if(tier2_client_verifylogin(read_buffer,bytesRead)){
			state = state_server_verified;
		}else{
			WARN("User verification failed");
			state = state_server_unverified;
		}
		break;
	case state_server_unverified:
	case state_server_verified:
		// should send update to server!
		if(bytesRead > 0 && read_buffer[0] != '#'){
			dump_server_messages(read_buffer,bytesRead);
		}
		break;
	default:
		//printf(">IS: %s",read_buffer);
		break;
	}

	return 0;
}

int t2_connector_send(const char* data,size_t len){
	int rc;
	if(sockfd < 0){
		return -1;
	}
	rc = send(sockfd,data,len,0);
	if(rc < 0){
		// something wrong
		ERROR("*** send(): %s.", strerror(errno));
	}
	return rc;
}

int t2_connector_publish(const char* message, size_t len){
	if(state != state_server_verified){
		if(state == state_server_unverified)
			INFO("publish message aborted because callsign %.9s is NOT verified.",config.callsign);
		else
			INFO("publish message aborted because tier2 connector is not ready yet.");
		return -1;
	}
	return t2_connector_send(message,len);
}

// keep alive
static void tier2_client_keepalive() {
	DBG("Sending keep-alive command.");
	t2_connector_send(KEEPALIVE_CMD,strlen(KEEPALIVE_CMD));
	last_keepalive = time(NULL);
}
