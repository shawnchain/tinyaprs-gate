/*
 * utils.h
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */

#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include <stdio.h>
#include <sys/types.h>
#include <stddef.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#ifdef __APPLE__
#include <sys/filio.h>
#endif

#define bool char
#define true 1
#define false 0
#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))

struct sockaddr_inx {
	union {
		struct sockaddr sa;
		struct sockaddr_in in;
		struct sockaddr_in6 in6;
	};
};

#define port_of_sockaddr(s) ((s)->sa.sa_family == AF_INET6 ? (s)->in6.sin6_port : (s)->in.sin_port)
#define addr_of_sockaddr(s) ((s)->sa.sa_family == AF_INET6 ? (void *)&(s)->in6.sin6_addr : (void *)&(s)->in.sin_addr)
#define sizeof_sockaddr(s)  ((s)->sa.sa_family == AF_INET6 ? sizeof((s)->in6) : sizeof((s)->in))

int resolve_hostname(const char *pair, struct sockaddr_inx *sa);

static inline int set_nonblock(int sockfd) {
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0) | O_NONBLOCK) == -1)
		return -1;
	return 0;
}

static inline void hexdump(void *d, size_t len) {
	unsigned char *s;
	for (s = d; len; len--, s++)
		printf("%02x ", (unsigned int) *s);
	printf("\n");
}

void stringdump(void *d, size_t len);


int do_daemonize(void);

static inline size_t bytes_available(int fd){
	int bytes_avail = 0;
	return ioctl(fd, FIONREAD, &bytes_avail);
	return bytes_avail;
}

time_t get_time_milli_seconds();

//////////////////////////////////////////////////////////////////
// Simple logger
#ifdef DEBUG
#define DBG(msg, ...)  _log("DEBUG",__FILE__,msg, ##__VA_ARGS__)
#else
#define DBG(msg, ...)
#endif
#define INFO(msg, ...) _log("INFO ",__FILE__,msg, ##__VA_ARGS__)
#define WARN(msg, ...) _log("WARN ",__FILE__,msg, ##__VA_ARGS__)
#define ERROR(msg, ...) _log("ERROR",__FILE__,msg, ##__VA_ARGS__)

void _log(const char* tag, const char* module, const char* message, ...);


//////////////////////////////////////////////////////////////////
// The poll wrapper
typedef enum{
	poll_state_idle = 0,
	poll_state_read,
	poll_state_write,
	poll_state_error
}poll_state;

typedef void (*poll_callback)(int,poll_state);
void poll_init();
int poll_add(int fd, poll_callback callback);
int poll_remove(int fd);
int poll_run();


//////////////////////////////////////////////////////////////////
// The BufferedReader wrapper
struct IOReader;

typedef void (*io_read_callback)(char*,size_t);
typedef int (*io_read_method)(/*reader*/struct IOReader*);
struct IOReader{
	int fd;
	char* buffer;
	size_t bufferLen;
	io_read_method fnRead; 		// the read implementation
	io_read_callback callback;	// the callback then something read

	// internal part
	size_t maxBufferLen;
	time_t timeout;				// read timeout
	time_t lastRead;
}Reader;

void io_init_linereader(struct IOReader *reader, int fd, char* buffer, size_t bufferLen,void* readercb);
void io_init_timeoutreader(struct IOReader *reader, int fd, char* buffer, size_t bufferLen,int timeout, void* readercb);
void io_run(struct IOReader *reader);
void io_close(struct IOReader *reader);

#endif /* SRC_UTILS_H_ */
