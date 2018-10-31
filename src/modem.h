/*
 * tnc_connector.h
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#ifndef MODEM_H_
#define MODEM_H_

#include <stdint.h>

struct AX25Msg;
typedef void (*modem_ax25_decode_callback)(struct AX25Msg*);

typedef struct{
	char 		path[32];
	int32_t		speed;
	char 		model[32];
	char 		firmware_rev[32];
	char 		hardware_rev[32];

	modem_ax25_decode_callback	ax25_callback;
}Modem;

extern Modem modem;
struct AX25Msg;



/**
 * Initialize the TNC Modem
 */
int modem_init(const char* path, int32_t speed, const char* model, char** initCmds, modem_ax25_decode_callback callback);

#endif /* MODEM_H_ */
