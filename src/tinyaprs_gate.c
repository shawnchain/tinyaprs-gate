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

Config config = {
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

int main(int argc, char* argv[]){
	int opt;
	while ((opt = getopt_long(argc, argv, "dh",
				long_opts, NULL)) != -1) {
		switch (opt){
		case 'd':
			config.in_background = true;
			break;
		case 'h':
			print_help(argc,argv);
			exit(0);
			break;
		case '?':
			exit(1);
		}
	}

	if (config.in_background){
		do_daemonize();
	}

	/*
	if (config.pid_file) {
		FILE *fp;
		if ((fp = fopen(config.pid_file, "w"))) {
			fprintf(fp, "%d\n", (int)getpid());
			fclose(fp);
		}
	}
	*/

	poll_init();
	int rc;
#define T2_CLIENT_MODULE 1
#if T2_CLIENT_MODULE
	rc = tier2_client_init(config.host,config.port,"foo","bar","");
	if(rc < 0){
		// igate init error
		ERROR("*** error initialize the APRS tier2 client, aborted.");
		exit(1);
	}
#endif
#ifdef __linux__
	const char* devName = "/dev/ttyUSB0";
	const char* model = "tinyaprs";
#else
	//const char* devName = "/dev/tty.usbserial";
	const char* devName = "/dev/tty.SLAB_USBtoUART";
	const char* model = "tinyaprs";
#endif
	rc = tnc_init(devName,9600,model,0);
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
