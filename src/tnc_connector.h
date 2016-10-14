/*
 * tnc_connector.h
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#ifndef SRC_TNC_CONNECTOR_H_
#define SRC_TNC_CONNECTOR_H_

#include <stdint.h>

typedef struct{
	char devname[32];
	int32_t  baudrate;
	char model[32];
	char firmware_rev[32];
	char board_rev[32];
}TNC;

extern TNC tnc;
struct AX25Msg;

typedef void (*tnc_ax25_decode_callback)(struct AX25Msg*);

/**
 * Initialize the TNC
 */
int tnc_init(const char* devname, int32_t baudrate, const char* model, char** initCmds, tnc_ax25_decode_callback callback);

/**
 *
 */
int tnc_run();

#endif /* SRC_TNC_CONNECTOR_H_ */
