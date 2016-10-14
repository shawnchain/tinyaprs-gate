/*
 * serial_port.c
 *
 *  Created on: 2016年9月2日
 *      Author: shawn
 */


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "utils.h"
#include "serial_port.h"

/**
 * Setup port with 8bits, no parity, 1 stop bit
 */
static int serial_port_setup(int fd, int speed){
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        ERROR("get serial port config failed, tcgetattr(): %s", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        ERROR("set serial port config failed, tcsetattr(): %s", strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * select supported baudrate
 */
static int _select_baudrate(int input){
	switch(input){
	case 9600:
		return B9600;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	default:
		return -1;
	}
}

int serial_port_open(const char* portname,int32_t baudrate){
	int fd;
#ifdef __APPLE__
	// open will block if O_NONBLOCK is not set
	fd = open(portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
#else
    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
#endif
    if (fd < 0) {
        ERROR("*** open() \"%s\": %s", portname, strerror(errno));
        return -1;
    }
    int speed = _select_baudrate(baudrate);
    if(speed < 0){
    	ERROR("*** unsupported baudrate %d",baudrate);
    	return -1;
    }
    if(serial_port_setup(fd,speed) < 0){
    	close(fd);
    	fd = -1;
    }

#ifdef __APPLE__
    serial_port_set_nonblock(fd,0);
#endif
    return fd;
}

int serial_port_set_nonblock(int fd,int nonblock){
	int flag = fcntl(fd, F_GETFD, 0);
	if(nonblock){
		flag |= O_NONBLOCK;
	}else{
		flag &= ~O_NONBLOCK;
	}
	return fcntl(fd, F_SETFL, flag);
}

#if 0
int serial_port_set_mincount(int fd,int mcount){
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        ERROR("*** tcgetattr(): %s", strerror(errno));
        return -1;
    }

    tty.c_cc[VMIN] = mcount ? 1 : 0;
    tty.c_cc[VTIME] = 5;        /* half second timer */

    if (tcsetattr(fd, TCSANOW, &tty) < 0){
        ERROR("*** tcsetattr(): %s", strerror(errno));
        return -1;
    }
    return 0;
}
#endif
