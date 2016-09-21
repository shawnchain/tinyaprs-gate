#include "tinyaprs_gate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "tier2_client.h"
#include "tnc_connector.h"
#include "beacon.h"
#include "ax25.h"
#include "config.h"
#include "log.h"

static AppConfig appConfig = {
		.in_background = false,
		.pid_file = "/tmp/tinyaprs.pid",
		.log_file = "/tmp/tinyaprs.log",
		.monitor_tnc = false,
};

static struct option long_opts[] = {
	{ "host", required_argument, 0, 'H'},
	{ "callsign", required_argument, 0, 'C'},
	{ "passcode", required_argument, 0, 'P'},
	{ "filter", required_argument, 0, 'F'},
	{ "device", required_argument, 0, 'D'},
	{ "symbol", required_argument, 0, 'S'},
	{ "location", required_argument, 0, 'L'},
	{ "text", required_argument, 0, 'T'},
	{ "monitor", no_argument, 0, 'M'},
	{ "log", required_argument, 0, 'l', },
	{ "daemon", no_argument, 0, 'd', },
	{ "help", no_argument, 0, 'h', },
	{ 0, 0, 0, 0, },
};

static void print_help(int argc, char *argv[]){
	printf("TinyAPRS iGate Daemon.\n");
	printf("Usage:\n");
	printf("  %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -H, --host                          hostanme of the APRS-IS server\n");
	printf("  -C, --callsign                      callsign for the APRS-IS connection\n");
	printf("  -P, --passcode                      passcode for the APRS-IS connection\n");
	printf("  -F, --filter                        receive filter for the APRS-IS connection\n");
	printf("  -D, --device                        specify tnc[0] device path\n"); /*-D /dev/ttyUSB0?AT+KISS=1;*/
	printf("  -S, --symbol                        set the beacon symbol (see APRS symbol table)\n");
	printf("  -L, --location                      set the beacon location (see APRS latlon format)\n");
	printf("  -T, --text                          set the beacon text\n");
	printf("  -M, --monitor                       print RF packets to STDOUT with TNC2 monitor format\n");
	printf("  -l, --log                           log file name\n");
	printf("  -d, --daemon                        run as daemon process\n");
	printf("  -h, --help                          print this help\n");

}

////////////////////////////////////////////////////////
//
// iGate Rules:
// SEE - http://ham.zmailer.org/oh2mqk/aprx/PROTOCOLS
//
static char APRS_RX_NO_RELAY_SRC[8][16] = {
		"NOCALL",
		"N0CALL",
		"WIDE",
		"TRACE",
		"TCP",
		""
};

static char APRS_RX_NO_RELAY_VIA[8][16] = {
		"RFONLY",
		"NOGATE",
		"N0GATE",
		"TCPIP",
		"TCPXX",
		""
};

static char APRS_RX_NO_RELAY_PAYLOAD_PREFIX[8][16] = {
		"?",
		"}",
		""
};

static int gate_ax25_message(AX25Msg* msg){
	if(msg->len == 0) return -1;
	if(msg->rpt_cnt == 8) return -1;

	// check if message could be gated
	int i = 0,j=0;
	for(i = 0;i < 5;i++){
		if(strncmp(APRS_RX_NO_RELAY_SRC[i],msg->src.call,6) ==0){
			DBG("message with src %.6s will not allowed to relay",msg->src.call);
			return -1;
		}
	}
	for(i = 0;i < 5;i++){
		for(j = 0;j<msg->rpt_cnt;j++){
			if(strncmp(APRS_RX_NO_RELAY_VIA[i],msg->rpt_lst[j].call,6) ==0){
				DBG("message with VIA path %.6s is no allowed to relay",msg->rpt_lst[j].call);
				return -1;
			}
		}
	}
	for(i = 0;i<2;i++){
		if(strstr((const char*)msg->info,APRS_RX_NO_RELAY_PAYLOAD_PREFIX[i]) == (const char*)(msg->info)){
			DBG("message payload started with %s will not allowed to relay",APRS_RX_NO_RELAY_PAYLOAD_PREFIX[i]);
			return -1;
		}
	}

	// append the path with
#if 0
	strncpy(msg->rpt_lst[msg->rpt_cnt].call,"TCPIP*",6);
	msg->rpt_cnt++;
#endif

	// print to TNC-2 monitor format and publish
	char txt[1024];
	int len = ax25_print(txt,1021,msg);
	if(tier2_client_publish(txt,len) < 0){
		return -1;
	}

	return 0;
}

/**
 * Callback method for tnc data received
 */
static void tnc_ax25_message_received(AX25Msg* msg){
	if(msg == NULL) return;

	// dump message
	char buf[2048];
	ax25_print(buf,sizeof(buf) - 1,msg);
	INFO("\n>From RF: %s",buf);

	// Monitor mode
	if(appConfig.monitor_tnc){
		return;
	}

	if(gate_ax25_message(msg) < 0){
		//DBG("TNC Received %d bytes",len);
		INFO("relay message to tier2 server failed");
	}else{
		INFO("relay message to tier2 server success");
	}
}

/*
 *  split the "LAT,LON"
 */
static void parse_location_arg(char* loc){
	char buf[32];
	int i = 0;
	char *lat = buf, *lon = 0;
	strncpy(buf,loc,sizeof(buf) - 1);
	while(buf[i] != 0){
		if(buf[i] == ','){
			buf[i] = 0;
			if(sizeof(buf) - 1 - i >= 9){ // at least 9 chars of longitude
				lon = buf + i + 1;
			}
			break;
		}
		i++;
	}

	if(lat && lon){
		strncpy(config.beacon.lat,lat,sizeof(config.beacon.lat)  - 1);
		strncpy(config.beacon.lon,lon,sizeof(config.beacon.lon)  - 1);
	}
}

int main(int argc, char* argv[]){
	int opt;
	while ((opt = getopt_long(argc, argv, "H:C:P:F:D:S:L:T:l:Mdh",
				long_opts, NULL)) != -1) {
		switch (opt){
		case 'H': // host
			strncpy(config.server, optarg, sizeof(config.server) -1);
			break;
		case 'C': // callsign
			strncpy(config.callsign, optarg, sizeof(config.callsign) - 1);
			break;
		case 'P': // passcode
			strncpy(config.passcode, optarg, sizeof(config.passcode) - 1);
			break;
		case 'F': // filter
			strncpy(config.filter, optarg, sizeof(config.filter) - 1);
			break;
		case 'D': // tnc device
			strncpy(config.tnc[0].device, optarg, sizeof(config.tnc[0].device) - 1);
			break;
		case 'S': // beacon symbol
			strncpy(config.beacon.symbol,optarg,sizeof(config.beacon.symbol) -1);
			break;
		case 'L':
			// location (XX,YY)
			parse_location_arg(optarg);
			break;
		case 'T': // beacon text
			strncpy(config.beacon.text,optarg,sizeof(config.beacon.text) -1);
			break;
		case 'l':
			strncpy(config.logfile,optarg,sizeof(config.logfile) - 1);
			break;
		case 'M':
			appConfig.monitor_tnc = true;
			break;
		case 'd':
			appConfig.in_background = true;
			break;
		case 'h':
			print_help(argc,argv);
			exit(0);
			break;
		case '?':
			exit(1);
		}
	}

	// disable background if monitor mode is on;
	if(appConfig.monitor_tnc){
		appConfig.in_background = false;
	}

	if (appConfig.in_background){
		do_daemonize();
	}

	FILE *fp;
	if ((fp = fopen(appConfig.pid_file, "w"))) {
		fprintf(fp, "%d\n", (int)getpid());
		fclose(fp);
	}

	int rc;

	if((rc = log_init(config.logfile)) < 0){
		printf("*** warning: log system initialize failed");
	}

	if((rc = config_init("/etc/tinyaprs.cfg"))<0){
		ERROR("*** error: initialize the configuration, aborted.");
		exit(1);
	}
	if((rc = poll_init()) < 0){
		ERROR("*** error: initialize the poll module, aborted.");
		exit(1);
	}
	if((rc = tnc_init(config.tnc[0].device,9600,config.tnc[0].model,NULL,tnc_ax25_message_received)) < 0){
		ERROR("*** error: initialize the TNC module, aborted.");
		exit(1);
	}

	// don't initialize tier2 connect and beacon under monitor mode
	if(appConfig.monitor_tnc){
		INFO("Running TNC Monitor");
	}else{
		if((rc = tier2_client_init(config.server)) < 0){
			ERROR("*** error initialize the APRS tier2 client, aborted.");
			exit(1);
		}
		if((rc = beacon_init()) < 0){
			ERROR("*** error initialize the Beacon module, aborted.");
			exit(1);
		}
	}

	// the main loop
	while(true){
		tnc_run();
		if(! appConfig.monitor_tnc){
			tier2_client_run();
			beacon_run();
		}
		poll_run();
	}
}
