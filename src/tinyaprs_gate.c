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

AppConfig appConfig = {
		.host = "t2xwt.aprs2.net",
		//.host = "127.0.0.1",
		.port = 14580,
		.in_background = false
};

static struct option long_opts[] = {
	{ "daemon", no_argument, 0, 'd', },
	{ "help", no_argument, 0, 'h', },
	{ 0, 0, 0, 0, },
};

static void print_help(int argc, char *argv[]){
	printf("Tiny APRS iGate Daemon.\n");
	printf("Usage:\n");
	printf("  %s [options]\n", argv[0]);
	printf("Options:\n");
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
		"TCPIP",
		"TCPXX",
		""
};

static char APRS_RX_NO_RELAY_PAYLOAD_PREFIX[8][16] = {
		"?",
		"}",
		""
};

/**
 * Callback method for tnc data received
 */
static void tnc_ax25_message_received(struct AX25Msg* msg){
	//DBG("TNC Received %d bytes",len);
	char buf[2048];
	ax25_print(buf,2047,msg);
	printf("---------------------------------------------------------------\n");
	printf("%s",buf);
	printf("---------------------------------------------------------------\n");
}

int main(int argc, char* argv[]){
	int opt;
	while ((opt = getopt_long(argc, argv, "dh",
				long_opts, NULL)) != -1) {
		switch (opt){
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

	/*
	if (appConfig.pid_file) {
		FILE *fp;
		if ((fp = fopen(config.pid_file, "w"))) {
			fprintf(fp, "%d\n", (int)getpid());
			fclose(fp);
		}
	}
	*/
	config_init(NULL);
	poll_init();
	int rc;

	rc = tier2_client_init(config.server,config.port,"foo","bar","");
	if(rc < 0){
		// igate init error
		ERROR("*** error initialize the APRS tier2 client, aborted.");
		exit(1);
	}

	rc = tnc_init(config.tnc[0].port,9600,config.tnc[0].model,NULL,tnc_ax25_message_received);
	if(rc < 0){
		ERROR("*** error initialize the TNC module, aborted.");
		exit(1);
	}

	beacon_init();

	while(true){
		tier2_client_run();
		tnc_run();
		beacon_run();
		poll_run();
	}
}
