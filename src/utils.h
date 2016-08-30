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

int do_daemonize(void);


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



#endif /* SRC_UTILS_H_ */
