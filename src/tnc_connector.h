/*
 * tnc_connector.h
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#ifndef SRC_TNC_CONNECTOR_H_
#define SRC_TNC_CONNECTOR_H_

typedef struct{
	char devname[32];
	int  baudrate;
	char model[32];
	char firmware_rev[32];
	char board_rev[32];
}TNC;

typedef struct{
	int reopen_wait_time;
	int init_wait_time;
	int read_wait_time_ms;
	int keepalive_wait_time;
}TNCConfig;

extern TNC tnc;

/**
 * Initialize the TNC
 */
int tnc_init(const char* devname, int baudrate, const char* model, char** initCmds);

/**
 *
 */
int tnc_run();

#endif /* SRC_TNC_CONNECTOR_H_ */
