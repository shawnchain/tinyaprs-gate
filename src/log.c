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

static int log_open(bool overwrite){
	if(overwrite)
		logfile = fopen(logfileName,"w");
	else
		logfile = fopen(logfileName,"a");
	if(!logfile){
		printf("ERROR - open log file failed, %s\n",logfileName);
		return -1;
	}

	// get log file size if appending
	if(overwrite){
		logSize = 0;
	}else{
		// appending mode, get existing file size
		struct stat st;
		memset(&st,0,sizeof(struct stat));
		stat(logfileName,&st);
		logSize = st.st_size;
	}
	return 0;
}

static int log_rotate(){
	// close the log file
	if(!logfile) return -1;
	fclose(logfile);
	logfile = NULL;

	// TODO -
	// remove the $logfileName.1
	// rename to $logfileName.1
	char logfileName2[150];
	snprintf(logfileName2, sizeof(logfileName2) - 1, "%s.1",logfileName);
	rename(logfileName,logfileName2);
	// reopen with overwrite
	return log_open(to_overwrite);
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
		if(logSize >= logSizeThreshold){
			// we should rotate the log right now!
			log_rotate();
		}
	}
}
