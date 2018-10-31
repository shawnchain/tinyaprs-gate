/*
 * beacon.c
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#include "beacon.h"

#include "utils.h"
#include "config.h"
#include "tier2_client.h"

#include <libubox/uloop.h>
#include <string.h>
#include <assert.h>

typedef struct APRSMsg{
	char type[1];
	char symbol[3];
	char lat[10];
	char lon[10];
	char text[256];
}APRSMsg;

#define BEACON_INTERVAL 300

static struct tier2_client *client;

// TODO - configurable beacon message
static char* DST = "APTBGW";
static APRSMsg aprs = {
		.type = "!",
		/*
		//.symbol="/r",
		.symbol="R&",
		.lat = "3012.48N",
		.lon = "12008.48E",
		.text = "431.040MHz iGate/TinyAPRS"
		*/
};
//#define BEACON_TEXT "BG5HHP-7>APTI01,TCPIP*:!3012.48N/12008.48Er431.040MHz iGate/TinyAPRS\r\n"

static struct uloop_timeout timer;

static int beacon_run();
static void on_timer_update(struct uloop_timeout *t) {
	beacon_run();
	uloop_timeout_set(t,BEACON_INTERVAL * 1000);
}

/*
 * print APRS message to string
 */
static int aprs_print(char* buf, size_t len, APRSMsg* aprs){
	return snprintf(buf,len,"%c%.8s%c%.9s%c%s",aprs->type[0],aprs->lat,aprs->symbol[0],aprs->lon,aprs->symbol[1],aprs->text);
}

int beacon_init(struct tier2_client *c){
	assert(c);
	client = c;

	memset(&timer,0,sizeof(timer));
	timer.cb = on_timer_update;
	uloop_timeout_set(&timer, 30 * 1000); // first beacon starts in next 30s

	// copy the configurations
	strncpy(aprs.symbol,config.beacon.symbol,sizeof(aprs.symbol) -1);
	strncpy(aprs.lat,config.beacon.lat,sizeof(aprs.lat) -1);
	strncpy(aprs.lon,config.beacon.lon,sizeof(aprs.lon) -1);
	strncpy(aprs.text,config.beacon.text,sizeof(aprs.text) -1);

	DBG("Beacon Init");
	DBG("  Location: %s,%s",aprs.lat,aprs.lon);
	DBG("  Symbol: %.2s",aprs.symbol);
	DBG("  Text: %s",aprs.text);
	return 0;
}

static int beacon_run(){
	if (!tier2_client_is_verified(client)) {
		return 0;
	}
	// BEACONING
	char payload[1024];
	int i = sprintf(payload,"%s>%s,TCPIP*:",config.callsign,DST);
	i += aprs_print(payload + i,1024 - i - 1,&aprs);
	payload[i++] = '\r';
	payload[i++] = '\n';
	payload[i] = '\0';
	DBG("\n>Beaconing: %.*s",i - 2,payload);
	int rc = tier2_client_publish(client, payload,i);
	if(rc < 0){
		DBG("Beaconing failed, %d",rc);
	}
	return 0;
}
