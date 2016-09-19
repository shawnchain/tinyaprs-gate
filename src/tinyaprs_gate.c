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

static AppConfig appConfig = {
		.in_background = false,
		.pid_file = "/tmp/tinygate.pid",
};

static struct option long_opts[] = {
	{ "callsign", required_argument, 0, 'C'},
	{ "passcode", required_argument, 0, 'P'},
	{ "device", required_argument, 0, 'D'},
	{ "daemon", no_argument, 0, 'd', },
	{ "help", no_argument, 0, 'h', },
	{ 0, 0, 0, 0, },
};

static void print_help(int argc, char *argv[]){
	printf("Tiny APRS iGate Daemon.\n");
	printf("Usage:\n");
	printf("  %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -C, --callsign                      iGate callsign\n");
	printf("  -P, --passcode                      iGate passcode\n");
	printf("  -D, --device                        tnc[0] device path\n");
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

	// append the path with TCPIP*
	strncpy(msg->rpt_lst[msg->rpt_cnt].call,"TCPIP*",6);
	msg->rpt_cnt++;

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
	if(gate_ax25_message(msg) < 0){
		INFO("relay message to tier2 server failed, message dump:");
		//DBG("TNC Received %d bytes",len);
		char buf[2048];
		ax25_print(buf,2047,msg);
		printf("Message Dump\n");
		printf("---------------------------------------------------------------\n");
		printf("%s",buf);
		printf("---------------------------------------------------------------\n");
	}else{
#ifdef DEBUG
		INFO("relay message to tier2 server success, message dump:");
		char buf[2048];
		ax25_print(buf,2047,msg);
		printf("Message Dump\n");
		printf("---------------------------------------------------------------\n");
		printf("%s",buf);
		printf("---------------------------------------------------------------\n");
#else
		INFO("relay message to tier2 server success");
#endif
	}
}

int main(int argc, char* argv[]){
	int opt;
	while ((opt = getopt_long(argc, argv, "C:P:D:dh",
				long_opts, NULL)) != -1) {
		switch (opt){
		case 'C':
			strncpy(config.callsign, optarg, sizeof(config.callsign) - 1);
			break;
		case 'P':
			strncpy(config.passcode, optarg, sizeof(config.passcode) - 1);
			break;
		case 'D':
			strncpy(config.tnc[0].device, optarg, sizeof(config.tnc[0].device) - 1);
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
	if (appConfig.in_background){
		do_daemonize();
	}
	FILE *fp;
	if ((fp = fopen(appConfig.pid_file, "w"))) {
		fprintf(fp, "%d\n", (int)getpid());
		fclose(fp);
	}


	int rc;
	if((rc = config_init("/etc/tinyaprs.cfg"))<0){
		ERROR("*** error initialize the configuration, aborted.");
		exit(1);
	}

	if((rc = poll_init()) < 0){
		ERROR("*** error initialize the poll module, aborted.");
		exit(1);
	}
	if((rc = tier2_client_init(config.server,config.port,"foo","bar","")) < 0){
		ERROR("*** error initialize the APRS tier2 client, aborted.");
		exit(1);
	}
	if((rc = tnc_init(config.tnc[0].device,9600,config.tnc[0].model,NULL,tnc_ax25_message_received)) < 0){
		ERROR("*** error initialize the TNC module, aborted.");
		exit(1);
	}
	if((rc = beacon_init()) < 0){
		ERROR("*** error initialize the Beacon module, aborted.");
		exit(1);
	}

	// the main loop
	while(true){
		tier2_client_run();
		tnc_run();
		beacon_run();
		poll_run();
	}
}
