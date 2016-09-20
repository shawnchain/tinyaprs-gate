/*
 * config.h
 *
 *  Created on: 2016年9月7日
 *      Author: shawn
 */

#ifndef SRC_CONFIG_H_
#define SRC_CONFIG_H_

#include <stdint.h>

/* SAMPLE CONFIG
{
    server="rotate.aprs.org:14580"
    port=14580,
    mycall="N0CALL-0",
    pass="-1",
    tnc=[
    	{
    		id:1
    		name:"tnc-vhf"
    		model:"tinyaprs",
    		port:"/dev/ttyS0",
    	}
    	{
    		id:2
    		name:"tnc-vhf"
    		model:"tinyaprs",
    		port:"/dev/ttyUSB0",
    	}

    ],
    beacon_text="",
    beacon={
        symbol="",
        lat="",
        lon="",
        text="",
        phg="",
    },
}
*/
typedef struct{
	char symbol[3];
	char lat[10];
	char lon[10];
	char phg[10];
	char text[256];
}BeaconConfig;

typedef struct{
	uint8_t id;
	char name[32];
	char model[16];
	char device[64];

	int32_t reopen_wait_time;
	int32_t init_wait_time;
	int32_t read_wait_time_ms;
	int32_t keepalive_wait_time;
}TNCConfig;


#define SUPPORTED_TNC_NUM 4

typedef struct _Config{
	char server[64];
	char callsign[16];
	char passcode[16];
	char filter[128];

	TNCConfig tnc[SUPPORTED_TNC_NUM];
	char beacon_text[256];

	char logfile[128];
}Config;

extern Config config;

int config_init(const char* f);


#endif /* SRC_CONFIG_H_ */
