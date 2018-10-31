/*
 * utils.h
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */

#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include "log.h"

#include <sys/types.h>
#include <sys/time.h>
#include <stddef.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#ifdef __APPLE__
#include <sys/filio.h>
#endif

#include <stdbool.h>
#include <stdio.h>

#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Misc definitions */
#ifndef NULL
#define NULL  (void *)0
#endif
#ifndef EOF
#define	EOF   (-1)
#endif


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

int resolve_host(const char *hostname_port_pair, struct sockaddr_inx *sa);

static inline int set_nonblock(int fd,bool nonblock){
	int flag = fcntl(fd, F_GETFD, 0);
	if(nonblock){
		flag |= O_NONBLOCK;
	}else{
		flag &= ~O_NONBLOCK;
	}
	return fcntl(fd, F_SETFL, flag);
}

static inline bool can_write(int fd){
	fd_set wset;
	struct timeval timeo;
	int rc;

	FD_ZERO(&wset);
	FD_SET(fd, &wset);
	timeo.tv_sec = 0;
	timeo.tv_usec = 0;
	rc = select(fd + 1, NULL, &wset, NULL, &timeo);
	if(rc >0 && FD_ISSET(fd, &wset)){
		return true;
	}
	return false;
}

void hexdump(void *d, size_t len);
void stringdump(void *d, size_t len);

char* string_trim_crlf_r(char *src, size_t len);

char * string_trim_space(char *str);

int do_daemonize(void);

static inline size_t bytes_available(int fd){
	int bytes_avail = 0;
	return ioctl(fd, FIONREAD, &bytes_avail);
	return bytes_avail;
}

time_t get_time_milli_seconds();

//static void aprs_calc_phgr(uint16_t txPower,uint16_t antGain,uint16_t txIerval,uint16_t hightAGL,uint16_t antDir,char* comments, char* out, size_t len);
void aprs_calc_location(char* latlon,char* out,size_t len);
short aprs_calc_hash(const char* thecall);

#endif /* SRC_UTILS_H_ */
