/*
 * log.h
 *
 *  Created on: 2016年9月20日
 *      Author: shawn
 */

#ifndef SRC_LOG_H_
#define SRC_LOG_H_

#include <sys/types.h>

//////////////////////////////////////////////////////////////////
// Simple logger
#ifdef DEBUG
#define DBG(msg, ...)  log_log("DEBUG",__FILE__,msg, ##__VA_ARGS__)
#else
#define DBG(msg, ...)
#endif
#define INFO(msg, ...) log_log("INFO ",__FILE__,msg, ##__VA_ARGS__)
#define WARN(msg, ...) log_log("WARN ",__FILE__,msg, ##__VA_ARGS__)
#define ERROR(msg, ...) log_log("ERROR",__FILE__,msg, ##__VA_ARGS__)

int log_fd();
int log_init(const char* logfile);
void log_log(const char* tag, const char* module, const char* message, ...);
void log_hexdump(void *d, size_t len);
#endif /* SRC_LOG_H_ */
