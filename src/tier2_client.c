/*
 * tier2_client.c
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */
#include "tier2_client.h"

#include "config.h"

#include "slre.h"

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

typedef enum{
	state_disconnected = 0,
	state_connected,
	state_server_prompt,
	state_server_unverified,
	state_server_verified,
}tier2_client_state;

static int 	tier2_client_open(struct tier2_client *c);
static int 	tier2_client_close(struct tier2_client *c);
static int 	tier2_client_receive(struct tier2_client *c, char* data, int len);
static void tier2_client_keepalive(struct tier2_client *c);
static int 	tier2_client_run(struct tier2_client *c);

static void timeout_handler(struct uloop_timeout *t){
	struct tier2_client *c = container_of(t, struct tier2_client, timer);
	tier2_client_run(c);
	uloop_timeout_set(t,5);
}

static void on_stream_read(struct xstream *x, char *data, int len){
	if (len == 0){
		// ignore blank line
		return;
	}
	struct tier2_client *c = container_of(x, struct tier2_client, stream);
	tier2_client_receive(c, data, len);
}

static void on_stream_write(struct xstream *x, int len){
	DBG("%d bytes written, %d left", len, x->stream_fd.stream.w.data_bytes);
}

static void on_stream_error(struct xstream *x){
	struct tier2_client *c = container_of(x, struct tier2_client, stream);
	tier2_client_close(c);
}

int tier2_client_init(struct tier2_client *c, const char* _host){
	assert(c);

	memset(c, 0, sizeof(*c));
	c->sockfd = -1;

	c->timer.cb = timeout_handler;
	uloop_timeout_set(&c->timer,5);

	// copy the parameters
	strncpy(c->server, _host,63);

	int rc;
	rc = tier2_client_open(c);
	if(rc == -EAGAIN){
		// resolve ok but connect failed, will retry later
		WARN("Connect to '%s' temporarily failed, will re-connect later.", c->server);
	}else if(rc == -EINVAL){
		ERROR("*** Invalid server address: '%s'.", c->server);
		return -1;
	}else if(rc < 0){
		ERROR("*** Unable to connect to '%s'.", c->server);
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

static int tier2_client_run(struct tier2_client *c){
	time_t t = time(NULL);
	if(c->state == state_disconnected){
		// try to reconnect
		if(t - c->last_reconnect > current_reconnect_waittime){
			INFO("reconnecting...");
			current_reconnect_waittime += 15;
			tier2_client_open(c);
		}
	}else{
		if(t - c->last_recv > IDLE_TIMEOUT){
			// try to reconnect as we have 90s idle, as the server will push every 20 seconds
			INFO("IDLE timeout, reconnecting...");
			tier2_client_close(c);
			tier2_client_open(c);
		}
		if(c->state == state_connected){
			if(current_reconnect_waittime != RECONNECT_WAITTIME){
				DBG("Reset current_reconnect_waittime");
				current_reconnect_waittime = RECONNECT_WAITTIME;
			}
		}
		if(c->state == state_server_verified || c->state == state_server_unverified){
			if(t - c->last_keepalive > KEEPALIVE_TIMEOUT){
				tier2_client_keepalive(c);
			}
		}
	}
	return 0;
}

static int tier2_client_open(struct tier2_client *c) {
	int rc;
	c->last_reconnect = time(NULL);
	c->last_keepalive = c->last_reconnect;

	if(c->state != state_disconnected){
		// already connected
		return 0;
	}

	INFO("Connecting to %s", c->server);

	if ((rc = resolve_host(c->server, &c->server_addr)) < 0) {
		// resolve host error, bail out
		WARN("resolve server %s failed, connect aborted.",c->server);
		return rc;
	}

	c->last_recv = time(NULL);

	if ((c->sockfd = socket((&c->server_addr)->sa.sa_family, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		ERROR("*** socket() failed: %s.", strerror(errno));
		return -1;
	}

	int optval = 1;
	if (setsockopt(c->sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int)) < 0){
	    WARN("*** can not set TCP_NODELAY option on  socket (%s)\n", strerror(errno));
	}

	if (connect(c->sockfd, (struct sockaddr *) &c->server_addr, sizeof_sockaddr(&c->server_addr)) < 0) {
		tier2_client_close(c);
		return -EAGAIN;
	}

	xstream_crlf_init(&c->stream,c->sockfd, 0, on_stream_read, on_stream_write, on_stream_error);
	c->stream.stream_fd.stream.string_data = true;

	set_nonblock(c->sockfd, true);
	//io_add(sockfd,tier2_client_poll_callback);

	c->state = state_connected;

	return 0;
}

static int tier2_client_close(struct tier2_client *c){
	if (c->state == state_disconnected)
		return 0;

	c->state = state_disconnected;
	if(c->sockfd >= 0){
		xstream_free(&c->stream);
		close(c->sockfd);
		c->sockfd = -1;
	}
	INFO("tier2_client_close()");
	return 0;
}

/**
 * logresp BG5HHP verified, server T2XWT
 */
static bool tier2_client_verifylogin(struct tier2_client *c, const char* resp, size_t len){
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

//const char* T2_LOGIN_CMD = "user TINYIS pass -1 vers TinyAprsGate 0.1 filter r/36.045101/103.836093/1500\r\n";
const char* T2_LOGIN_CMD = "user %s pass %s vers TinyAPRS 0.1 filter %s\r\n";
const char* T2_KEEPALIVE_CMD = "#TinyAprsGate 0.1\r\n";

/*
 * dump server message
 */
static void dump_server_messages(char* data, int len){
	printf(">From IS: %.*s\n", len, data);
}

static int tier2_client_receive(struct tier2_client *c, char* data, int len) {
	assert(len >0);
	
	c->last_recv = time(NULL);

	// removing the trailing <CR><LF>
	switch(c->state){
	case state_connected:
		// send out login cmd when server greetings received.
		INFO("Server Greeting: %s",string_trim_crlf_r(data,len));
		c->state = state_server_prompt;
		char loginCmd[512];
		int cmdLen = snprintf(loginCmd,511,T2_LOGIN_CMD,config.callsign,config.passcode,config.filter);
		INFO("Login Request: %.*s",cmdLen - 2 /*not logging the trailing CRLF chars*/, loginCmd);
		tier2_client_send(c, loginCmd,cmdLen); // send login command
		break;
	case state_server_prompt:
		//check  server login respond
		INFO("Server Respond: %s",string_trim_crlf_r(data,len));
		if(tier2_client_verifylogin(c, data,len)){
			c->state = state_server_verified;
		}else{
			WARN("User verification failed");
			c->state = state_server_unverified;
		}
		break;
	case state_server_unverified:
	case state_server_verified:
		// should send update to server!
		if(data[0] != '#'){
			dump_server_messages(data,len);
		}
		break;
	default:
		//printf(">IS: %s",data);
		break;
	}

	return 0;
}

int tier2_client_send(struct tier2_client *c, const char* data, size_t len){
	int rc = 0;
	if(c->sockfd < 0){
		return -1;
	}

	rc = ustream_write(&c->stream.stream_fd.stream, data, len, false);
	if (rc < 0){
		// something wrong
		ERROR("*** send(): %s.", strerror(errno));
	} else if (rc < len ){
		// write incomplete
		INFO("send() %d bytes, left %d", rc, c->stream.stream_fd.stream.w.data_bytes);
	}
	return rc;
}

int tier2_client_publish(struct tier2_client *c, const char* message, size_t len){
	if(c->state != state_server_verified){
		if(c->state == state_server_unverified)
			INFO("publish message aborted because callsign %.9s is NOT verified.",config.callsign);
		else
			INFO("publish message aborted because tier2 connector is not ready yet.");

		return -1;
	}
	return tier2_client_send(c, message,len);
}

bool tier2_client_is_verified(struct tier2_client *c){
	return c->state == state_server_verified;
}

// keep alive
static void tier2_client_keepalive(struct tier2_client *c) {
	DBG("Sending keep-alive command.");
	tier2_client_send(c, T2_KEEPALIVE_CMD,strlen(T2_KEEPALIVE_CMD));
	c->last_keepalive = time(NULL);
}
