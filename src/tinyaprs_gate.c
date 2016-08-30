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

	int rc = tier2_client_init(config.host,config.port,"foo","bar","");
	if(rc < 0){
		// igate init error
		printf("ERROR initialize the APRS tier2 client, aborted\n");
		exit(1);
	}

	while(true){
		tier2_client_run();
		poll_run();
	}
}
