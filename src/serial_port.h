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
int serial_port_open(const char* devName,int32_t baudrate);

int serial_port_set_nonblock(int fd,int nonblock);

#endif /* SRC_SERIAL_PORT_H_ */
