/*
 * serial_port.h
 *
 *  Created on: 2016年9月2日
 *      Author: shawn
 */

#ifndef SRC_SERIAL_PORT_H_
#define SRC_SERIAL_PORT_H_

/**
 * Open serial port
 */
int serial_open(const char* devName,int32_t baudrate);

#endif /* SRC_SERIAL_PORT_H_ */
