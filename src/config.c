/*
 * config.c
 *
 *  Created on: 2016年9月7日
 *      Author: shawn
 */


#include "config.h"

Config config = {
		.server = "t2xwt.aprs2.net",
		.port = 14580,
		.mycall="NOCALL",
		.pass="-1",
		.filter="r/30.2731/120.1543/15",

		.tnc ={
				{
					#ifdef __linux__
					.port="/dev/ttyUSB0",
					#else
					.port="/dev/tty.SLAB_USBtoUART",
					#endif
					.model="tinyaprs",
					.reopen_wait_time = 15,
					.init_wait_time = 3,
					.read_wait_time_ms = 350,
					.keepalive_wait_time = -1,
				},
				{
					.port="",
					.model="",
				},
				{
					.port="",
					.model="",
				},
				{
					.port="",
					.model="",
				},
			}
		,

		.beacon_text="!3012.48N/12008.48Er431.040MHz iGate/TinyAPRS",
};

/**
 * Load config file
 */
int config_init(const char* f){
	//TODO - read from config file
	return 0;
}
