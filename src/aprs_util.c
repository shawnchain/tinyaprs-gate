/*
 * aprs_util.c
 *
 *  Created on: 2016年9月27日
 *      Author: shawn
 */

#ifndef SRC_APRS_UTIL_C_
#define SRC_APRS_UTIL_C_

#include <string.h>
#include <strings.h>
#include "utils.h"
#include "log.h"
#include "hash.h"

//#ifdef APRS_UTIL_STANDALONE
static void aprsutil_usage(int argc, char* argv[]){
	printf("APRS Utils\n");
	printf("    aprsutil location LAT,LON\t\tConvert the APRS location\n");
	printf("    aprsutil hash CALLSIGN\t\tPrint the passcode\n");
}

static void aprsutil_location(int argc, char* argv[]){
	char buf[64];
	aprs_calc_location(argv[2],buf,sizeof(buf) -1);
	printf("APRS Location: %s\n",buf);
}

static void aprsutil_hash(int argc, char* argv[]){
	char* callsign = argv[1];
	short hash = aprs_calc_hash(callsign);
	printf("%s -> %d\n",callsign,hash);
}

static void aprsutil_md5(int argc, char* argv[]){
	unsigned char digest[16];
	memset(digest,0,16);
	if(hash_md5_file(argv[2],digest) < 0){
		printf("%s: No such file\n",argv[2]);
		return;
	}
	printf("MD5(%s)= ",argv[2]);
	int i = 0;
	for(i = 0;i<16;i++){
		printf("%02x",digest[i]);
	}
	printf("\n");
}

int main(int argc, char* argv[]) {
	if(argc < 3) {
		aprsutil_usage(argc,argv);
	}else if(strncmp("location",argv[1],8) == 0){
		aprsutil_location(argc,argv);
	}else if(strncmp("hash",argv[1],4) == 0){
		aprsutil_hash(argc,argv);
	}else if(strncmp("md5",argv[1],3) == 0){
		aprsutil_md5(argc,argv);
	}
	return 0;
}

#endif /* SRC_APRS_UTIL_C_ */
