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
	bool in_background;
	char pid_file[64];
}AppConfig;

#endif /* SRC_TINYAPRS_GATE_H_ */
