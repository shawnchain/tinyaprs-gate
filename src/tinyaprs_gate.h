/*
 * tinyaprs_gate.h
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */

#ifndef SRC_TINYAPRS_GATE_H_
#define SRC_TINYAPRS_GATE_H_


#include "utils.h"

typedef struct {
	char host[40];
	unsigned short port;
	bool in_background;
}AppConfig;

extern AppConfig appConfig;

#endif /* SRC_TINYAPRS_GATE_H_ */
