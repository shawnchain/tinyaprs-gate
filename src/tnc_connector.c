/*
 * tnc_connector.c
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#include "tier2_client.h"
#include "utils.h"

static void tnc_poll_callback(int fd, poll_state state){

}

int tnc_init(const char* devname, unsigned int baudrate){
	//TODO - open the tnc serial port, send the initialize command

	return 0;
}

int tnc_run(){
	//TODO - read
	return 0;
}
