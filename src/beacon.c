/*
 * beacon.c
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#include "tier2_client.h"

#include <string.h>
#include <time.h>
#include "tier2_client.h"
#include "utils.h"
#include "config.h"

typedef struct APRSMsg{
	char type[1];
	char symbol[2];
	char lat[8];
	char lon[9];
	char text[256];
}APRSMsg;

// TODO - configurable beacon message
static char* SRC = "BG5HHP-7";
static char* DST = "APTI01";
static APRSMsg aprs = {
		.type = "!",
		//.symbol="/r",
		.symbol="R&",
		.lat = "3012.48N",
		.lon = "12008.48E",
		.text = "431.040MHz iGate/TinyAPRS"
};
//#define BEACON_TEXT "BG5HHP-7>APTI01,TCPIP*:!3012.48N/12008.48Er431.040MHz iGate/TinyAPRS\r\n"

static time_t last_beacon = 0;

/*
 * print APRS message to string
 */
static int aprs_print(char* buf, size_t len, APRSMsg* aprs){
	return snprintf(buf,len,"%c%.8s%c%.9s%c%s",aprs->type[0],aprs->lat,aprs->symbol[0],aprs->lon,aprs->symbol[1],aprs->text);
}

int beacon_init(){
	last_beacon = time(NULL) - 300 + 30;
	return 0;
}

int beacon_run(){
	time_t t = time(NULL);
	if(t - last_beacon > (5 * 60)){
		// BEACONING
		char payload[1024];
		int i = sprintf(payload,"%s>%s,TCPIP*:",SRC,DST);
		i += aprs_print(payload + i,1024 - i - 1,&aprs);
		payload[i++] = '\r';
		payload[i++] = '\n';
		DBG("Beaconing: %s",payload);
		int rc = tier2_client_publish(payload,i);
		if(rc < 0){
			DBG("Beaconing failed, %d",rc);
		}
		last_beacon = t;
	}
	return 0;
}
