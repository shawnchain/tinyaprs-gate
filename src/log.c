/*
 * log.c
 *
 *  Created on: 2016年9月20日
 *      Author: shawn
 */
#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

static size_t logSizeThreshold = 128 * 1024;
static size_t logSize = 0;
//static unsigned char logLevelThreshold = 0;
static char logfileName[128];
static FILE *logfile = NULL;

#define to_append false
#define to_overwrite true

int log_fd(){
	if(logfile){
		return fileno(logfile);
	}else{
		return -1;
	}
}

static int log_open(bool overwrite){
	if(overwrite)
		logfile = fopen(logfileName,"w");
	else
		logfile = fopen(logfileName,"a");
	if(!logfile){
		//log_log("ERROR",__FILE__,"open log file failed, %s",logfileName);
		printf("open log file failed, %s\n",logfileName);
		return -1;
	}

	// get log file size if appending
	if(overwrite){
		logSize = 0;
		printf("open log file [%s] success\n",logfileName);
	}else{
		// appending mode, get existing file size
		struct stat st;
		memset(&st,0,sizeof(struct stat));
		stat(logfileName,&st);
		logSize = st.st_size;
		//log_log("INFO ",__FILE__,"log file size, %d",logSize);
		printf("open log file %s success, file size: %d\n",logfileName,(int)logSize);
	}
	return 0;
}

static int log_rotate(){
	int rc = 0;
	// close the log file
	if(!logfile) return -1;
	fclose(logfile);
	logfile = NULL;

	// TODO -
	// remove the $logfileName.1
	// rename to $logfileName.1
	char logfileName2[150];
	snprintf(logfileName2, sizeof(logfileName2) - 1, "%s.1",logfileName);
	// remove the existing old file
	remove(logfileName2);
	rename(logfileName,logfileName2);
	// reopen with overwrite
	//log_log("INFO ",__FILE__,"log file rotated");
	printf("log file rotated\n");
	rc = log_open(to_overwrite);
	return rc;
}

int log_init(const char* logfile){
	int rc;
	strncpy(logfileName,logfile,sizeof(logfileName) - 1);
	if((rc = log_open(to_append)) < 0){
		return rc;
	}
	return 0;
}

void log_log(const char* tag, const char* module, const char* msg, ...) {
	char string[1024];
	va_list args;
	va_start(args,msg);

	size_t bytesPrinted = 0;

	char stime[32];
	time_t current_time;
	struct tm * time_info;
	time(&current_time);
	time_info = localtime(&current_time);
	strftime(stime, sizeof(stime) -1, "%Y-%m-%d %H:%M:%S", time_info);

	// head
	bytesPrinted = sprintf(string,"%s [%s] (%s) - ",stime,tag,module);
	// body
	bytesPrinted += vsnprintf((string + bytesPrinted),sizeof(string) - bytesPrinted - 1,msg,args);

	if(logfile){
		fprintf(logfile,"%s\n", string);
		fflush(logfile);
	}
	// if debug level is on, pring to console as well
	if(strncmp("ERROR",tag,5) == 0){
		fprintf(stderr,"%s\n", string);
	}else{
		printf("%s\n",string);
	}
	bytesPrinted++; // including the leading '\n'

	if(bytesPrinted > 0){
		logSize += bytesPrinted;
		//printf("logsize: %d\n",(int)logSize);
		if(logSize >= logSizeThreshold){
			// we should rotate the log right now!
			log_rotate();
		}
	}
}

void log_hexdump(void *d, size_t len) {
	unsigned char *s;
	size_t bytesPrinted = 0;
	if(logfile){
		for (s = d; len; len--, s++){
			bytesPrinted += fprintf(logfile,"%02x ", (unsigned int) *s);
		}
		bytesPrinted += fprintf(logfile,"\n");
		fflush(logfile);
	}
	for (s = d; len; len--, s++){
		printf("%02x ", (unsigned int) *s);
	}
	printf("\n");
}
