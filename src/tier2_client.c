/*
 * tier2_client.c
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include "tinyaprs_gate.h"
#include "utils.h"
#include "slre.h"

#include "tier2_client.h"
#include "config.h"

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

static void tier2_client_poll_callback(int fd, poll_state state){
	switch(state){
		case poll_state_read:
			tier2_client_receive(fd);
			break;
		case poll_state_write:
			//TODO -flush the send buffer queue
			break;
		case poll_state_error:
			tier2_client_disconnect();
			break;
		case poll_state_idle:
			break;
		default:
			break;
	}
}

static char host[50], user[10], pass[6],filter[512];
static unsigned short port;

int tier2_client_init(const char* _host, unsigned short _port, const char* _user, const char* _pass, const char* _filter){
	// copy the parameters
	strncpy(host,_host,49);
	strncpy(user,_user,9);
	strncpy(pass,_pass,5);
	strncpy(filter,_filter,511);
	port = _port;

	INFO("Connecting to %s:%u", host, port);

	int rc;
	rc = tier2_client_connect();
	if(rc == -EAGAIN){
		// resolve ok but connect failed, will retry later
		WARN("Connect to '%s' temporarily failed, will re-connect later.", host);
	}else if(rc == -EINVAL){
		ERROR("*** Invalid address pair '%s'.", host);
		return -1;
	}else if(rc < 0){
		ERROR("*** Unable to connect to '%s'.", host);
		return -1;
	}else{
		INFO("Connected to %s:%u", host, port);
	}

	return 0;
}

#define RECONNECT_WAITTIME 10
#define KEEPALIVE_TIMEOUT 60
#define IDLE_TIMEOUT 90

int tier2_client_run(){
	time_t t = time(NULL);
	if(state == state_disconnected){
		// try to reconnect
		if(t - last_reconnect > RECONNECT_WAITTIME){
			INFO("reconnecting...");
			tier2_client_connect();
		}
	}else{
		if(t - last_recv > IDLE_TIMEOUT){
			// try to reconnect as we have 90s idle, as the server will push every 20 seconds
			INFO("IDLE timeout, reconnecting...");
			tier2_client_disconnect();
			tier2_client_connect();
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

	if ((rc = resolve_hostname(host, &server_addr)) < 0) {
		// resolve host error, bail out
		WARN("resolve hostname %s failed, connect aborted.",host);
		return rc;
	}

	last_recv = time(NULL);
	char s_server_addr[50];
	inet_ntop(server_addr.sa.sa_family, addr_of_sockaddr(&server_addr),
			s_server_addr, sizeof(s_server_addr));
	INFO("Resolved to %s:%u", s_server_addr, ntohs(port_of_sockaddr(&server_addr)));

	if ((sockfd = socket((&server_addr)->sa.sa_family, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		ERROR("*** socket() failed: %s.", strerror(errno));
		return -1;
	}

	if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof_sockaddr(&server_addr)) < 0) {
		tier2_client_disconnect();
		return -EAGAIN;
	}
	set_nonblock(sockfd);
	poll_add(sockfd,tier2_client_poll_callback);

	state = state_connected;

	return 0;
}

static int tier2_client_disconnect(){
	state = state_disconnected;
	if(sockfd >= 0){
		poll_remove(sockfd);
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
const char* LOGIN_CMD = "user %s pass %s vers TinyAprsGate 0.1 filter %s\r\n";
const char* KEEPALIVE_CMD = "#TinyAprsGate 0.1\r\n";

static int tier2_client_receive(int _sockfd) {
	int rc;
	char read_buffer[buffer_len];
	bzero(&read_buffer,buffer_len);
	rc = recv(_sockfd,&read_buffer, buffer_len,0);
	if(rc <=0){
		// socket closed or something wrong
		return -1;
	}

	printf(">%s",read_buffer);

	last_recv = time(NULL);

	switch(state){
	case state_connected:
		state = state_server_prompt;
		char loginCmd[512];
		int i = snprintf(loginCmd,511,LOGIN_CMD,config.callsign,config.passcode,config.filter);
		tier2_client_send(loginCmd,i); // send login command
		break;
	case state_server_prompt:
		//TODO - check  server login respond
		if(tier2_client_verifylogin(read_buffer,rc)){
			state = state_server_verified;
		}else{
			WARN("User verification failed");
			state = state_server_unverified;
		}
		break;
	case state_server_verified:
		// should send update to server!
		break;
	default:
		break;
	}

	return 0;
}

int tier2_client_send(const char* data,size_t len){
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

int tier2_client_publish(const char* message, size_t len){
	if(state != state_server_verified){
		INFO("user unverified, publish message aborted");
		return -1;
	}
	return tier2_client_send(message,len);
}

// keep alive
static void tier2_client_keepalive() {
	INFO("Sending keep-alive command.");
	tier2_client_send(KEEPALIVE_CMD,strlen(KEEPALIVE_CMD));
	last_keepalive = time(NULL);
}
