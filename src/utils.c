/*
 * utils.c
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/time.h>


#include "utils.h"

void _log(const char* tag, const char* module, const char* msg, ...) {
	char string[512];
	va_list args;
	va_start(args,msg);
	vsnprintf(string,511,msg,args);

	char stime[32];
	time_t current_time;
	struct tm * time_info;
	time(&current_time);
	time_info = localtime(&current_time);
	strftime(stime, 32, "%Y-%m-%d %H:%M:%S", time_info);

	if(strncmp("ERROR",tag,5) == 0){
		fprintf(stderr,"%s [%s] (%s) - %s\n", stime, tag, module, string);
		//fflush(stderr);
	}else{
		printf("%s [%s] (%s) - %s\n", stime, tag, module, string);
		//fflush(stdout);
	}
}

int resolve_hostname(const char *hostname, struct sockaddr_inx *sa) {
	struct addrinfo hints, *result;
	char s_port[10] = "";
	int port = 14580, rc;

	/* Only getting an INADDR_ANY address. */
	if (hostname == NULL) {
		return -EINVAL;
	}

//	if (sscanf(pair, "[%50[^]]]:%d", host, &port) == 2) {
//	} else if (sscanf(pair, "%50[^:]:%d", host, &port) == 2) {
//	} else {
//		/**
//		 * Address with a single port number, usually for
//		 * local IPv4 listen address.
//		 * e.g., "10000" is considered as "0.0.0.0:10000"
//		 */
//		const char *sp;
//		for (sp = pair; *sp; sp++) {
//			if (!(*sp >= '0' && *sp <= '9'))
//				return -EINVAL;
//		}
//		sscanf(pair, "%d", &port);
//		strcpy(host, "0.0.0.0");
//	}
	sprintf(s_port, "%d", port);
	if (port <= 0 || port > 65535)
		return -EINVAL;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;  /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;  /* For wildcard IP address */
	hints.ai_protocol = 0;        /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	if ((rc = getaddrinfo(hostname, s_port, &hints, &result)))
		return -EAGAIN;

	/* Get the first resolution. */
	memcpy(sa, result->ai_addr, result->ai_addrlen);

	freeaddrinfo(result);
	return 0;
}

int do_daemonize(void)
{
	pid_t pid;

	if ((pid = fork()) < 0) {
		fprintf(stderr, "*** fork() error: %s.\n", strerror(errno));
		return -1;
	} else if (pid > 0) {
		/* In parent process */
		exit(0);
	} else {
		/* In child process */
		int fd;
		setsid();
		chdir("/tmp");
		if ((fd = open("/dev/null", O_RDWR)) >= 0) {
			dup2(fd, 0);
			dup2(fd, 1);
			dup2(fd, 2);
			if (fd > 2)
				close(fd);
		}
	}
	return 0;
}

time_t get_time_milli_seconds(){
	struct timeval  tv;
	gettimeofday(&tv, NULL);
	time_t time_in_mill =
	         (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
	return time_in_mill;
}

void stringdump(void *d, size_t len){
	unsigned char *s;
	printf("=======================================================================\n");
	for (s = d; len; len--, s++)
		printf("%c", *s);
	printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
}

//////////////////////////////////////////////////////////////////
// Simple poll wrapper

#define pollfds_len 8
static int pollfds[pollfds_len];
static poll_callback pollcbs[pollfds_len];
static int maxfd = -1;

void poll_init(){
	for(int i = 0;i < pollfds_len;i++){
		pollfds[i] = -1;
		pollcbs[i] = 0;
	}
}

int poll_add(int fd, poll_callback callback){
	for(int i = 0;i < pollfds_len;i++){
		if(pollfds[i] == fd){
			// all ready there,
			return i;
		} else if(pollfds[i] < 0){
			pollfds[i] = fd;
			pollcbs[i] = callback;
			if(fd > maxfd){
				maxfd = fd;
			}
			DBG("Add fd %d to poll list, maxfd is %d",fd,maxfd);
			return i;
		}
	}
	return -1;
}

int poll_remove(int fd){
	for(int i = 0;i < pollfds_len;i++){
		if(pollfds[i] == fd){
			pollfds[i] = -1;
			pollcbs[i] = 0;
			if(maxfd == fd){
				// should update the maxfd
				for(int j = 0;j<pollfds_len;j++){
					if(pollfds[j] > maxfd){
						maxfd = pollfds[j];
					}
				}
			}
			DBG("Remove fd %d from poll list, maxfd is %d",fd,maxfd);
			return i;
		}
 	}

	return -1;
}

int poll_run(){
	fd_set rset,wset,eset;
	struct timeval timeo;
	int rc;

	FD_ZERO(&rset);
	FD_ZERO(&wset);
	FD_ZERO(&eset);

	for(int i = 0;i < pollfds_len; i++){
		if(pollfds[i] >=0){
			FD_SET(pollfds[i], &rset);
			FD_SET(pollfds[i], &wset);
			FD_SET(pollfds[i], &eset);
		}
	}

	timeo.tv_sec = 0;
	timeo.tv_usec = 500000; // 500ms

	rc = select(maxfd + 1, &rset, &wset, &eset, &timeo);
	if (rc < 0) {
		fprintf(stderr, "*** select(): %s.\n", strerror(errno));
		return -1;
	}else if(rc >0){
		// got ready
		for(int i = 0;i<pollfds_len;i++){
			if(pollfds[i] >=0){
				 if(FD_ISSET(pollfds[i], &rset) && pollcbs[i] > 0){
					 pollcbs[i](pollfds[i],poll_state_read);
				 }
				 if (FD_ISSET(pollfds[i], &wset) && pollcbs[i] > 0){
					 pollcbs[i](pollfds[i],poll_state_write);
				 }
				 if (FD_ISSET(pollfds[i], &eset) && pollcbs[i] > 0){
					 pollcbs[i](pollfds[i],poll_state_error);
				 }
			}
		}
	}else{
		// idle
		for(int i = 0;i<pollfds_len;i++){
			if(pollfds[i] >=0){
				 if(pollcbs[i] > 0){
					 pollcbs[i](pollfds[i],poll_state_idle);
				 }
			}
		}
		usleep(10000);
	}
	return 0;
}

