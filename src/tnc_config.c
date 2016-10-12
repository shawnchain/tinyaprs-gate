/*
 * tnc_config.c
 *
 *  Created on: 2016年10月11日
 *      Author: shawn
 */

#include "tnc_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <termios.h>

#include "utils.h"
#include "log.h"
#include "serial_port.h"

#define KISS_FEND  0xc0
#define KISS_FESC  0xdb
#define KISS_TFEND 0xdc
#define KISS_TFESC 0xdd

static int kiss_decode_ex(uint8_t *indata, uint16_t indata_len, uint8_t *ch, uint8_t *cmd, uint8_t *outdata, uint16_t *outdata_len) {
	uint16_t i = 0, o = 0;
	bool escaped = false;

	// byte 0 shold be 0xC0
	if (indata_len < 3 || indata[0] != KISS_FEND) {
		*outdata_len = 0;
		return 0;
	}
	// byte 1 should be the channels
	if(ch) *ch = indata[1] >> 4 & 0x0f;
	if(cmd) *cmd = indata[1] & 0x0f;
	i = 2;

	for (; i < indata_len; i++) {
		uint8_t c = indata[i];
		if (c == KISS_FEND) {
			// end flag
			break;
		} else if (c == KISS_FESC) {
			escaped = true;
		} else if (c == KISS_TFESC && escaped) {
			outdata[o++] = KISS_FESC;
			escaped = false;
		} else if (c == KISS_TFEND && escaped) {
			outdata[o++] = KISS_FEND;
			escaped = false;
		} else {
			outdata[o++] = c & 0xff;
			escaped = false; // reset the escape flag
		}
	}

	printf("Decoded %d bytes of data , port: %d, cmd: %d\n", o, *ch, *cmd);

	// we're done
	*outdata_len = o;
	return o;
}

static int kiss_encode_ex(uint8_t *indata, uint16_t indata_len, uint8_t ch, uint8_t cmd, uint8_t *outdata, uint16_t *outdata_len) {
	uint16_t i = 0, o = 0;
	outdata[o++] = KISS_FEND;
	outdata[o++] = (ch << 4 | cmd) ; // ch 0, cmd 0
	for (i = 0; i < indata_len && o < *outdata_len - 3; i++) {
		uint8_t c = indata[i];
		switch (c) {
		case KISS_FEND:
			outdata[o++] = KISS_FESC;
			outdata[o++] = KISS_TFEND;
			break;
		case KISS_FESC:
			outdata[o++] = KISS_FESC;
			outdata[o++] = KISS_TFESC;
			break;
		default:
			outdata[o++] = c;
		}
	}
	outdata[o++] = KISS_FEND;
	*outdata_len = o; //update the out data
	return o;
}

#if 0
/* --
 * C0 00
 * 82 A0 B4 9A 88 A4 60
 * 9E 90 64 90 A0 9C 72
 * 9E 90 64 A4 88 A6 E0
 * A4 8C 9E 9C 98 B2 61
 * 03 F0
 * 21 36 30 32 39 2E 35 30 4E 2F 30 32 35 30 35 2E 34 33 45 3E 20 47 43 53 2D 38 30 31 20
 * C0
 * --
 */
static uint8_t test_data_for_dec[] = {
		0xC0, 0x00,
		0x82, 0xA0, 0xB4, 0x9A, 0x88,
		0xA4, 0x60, 0x21, 0x36, 0x30, 0x32, 0x39, 0x2E,
		0x35, 0x30, 0x4E, 0x2F, 0x30, 0x32, 0x35, 0x30,
		0x35, 0x2E, 0x34, 0x33, 0x45, 0x3E, 0x20, 0x47,
		0x43, 0x53, 0x2D, 0x38, 0x30, 0x31, 0x20,
		0xC0 };

static uint8_t test_data_for_enc[] = {
		0x21, 0x36, 0x30, 0x32, 0x39, 0x2E, 0x35, 0x30,
		0x4E, 0x2F, 0x30, 0x32, 0x35, 0x30, 0x35, 0x2E,
		0x34, 0x33, 0x45, 0x3E, 0x20, 0x47, 0x43, 0x53,
		0x2D, 0x38, 0x30, 0x31, 0x20, };

#endif

static void _usage(int argc, char* argv[]) {
	printf("TNC Configuration Util\n");
	printf("    tncfg read /dev/ttyUSB0 \t\tRead configurations from TNC\n");
	printf("    tncfg write /dev/ttyUSB0 \t\tWrite configuration to TNC\n");
}

const char* defaultSerialPath = "/dev/tty.usbserial";
static int serial_fd = -1;

static bool serial_can_write(int fd, time_t timeout){
	fd_set wset;
	struct timeval timeo;
	int rc;

	FD_ZERO(&wset);
	FD_SET(fd, &wset);
	timeo.tv_sec = timeout;
	timeo.tv_usec = 0;
	rc = select(fd + 1, NULL, &wset, NULL, &timeo);
	if(rc >0 && FD_ISSET(fd, &wset)){
		return true;
	}
	return false;
}

/*
 * check and block 15s for data ready
 */
static bool serial_can_read(int fd, time_t timeout){
	fd_set rset;
	struct timeval timeo;
	int rc;

	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	timeo.tv_sec = timeout;
	timeo.tv_usec = 0;
	rc = select(fd + 1, NULL, &rset, NULL, &timeo);
	if(rc >0 && FD_ISSET(fd, &rset)){
		return true;
	}
	return false;
}

static int write_to_serial(uint8_t *buf, size_t buf_len,time_t timeout){
	int rc = 0;
	if(!serial_can_write(serial_fd,timeout)){
		return 0;
	}
	rc = write(serial_fd, buf, buf_len);
	if(rc > 0){
		tcdrain(serial_fd);
	}
	return rc;
}

/*
 * Poll serial port and trying to read
 *
 * return -1 if failed, 0 if timeout or >0 of actual bytes read.
 */
static int read_from_serial(uint8_t *buf,size_t buf_len,time_t timeout){
	int rc;

	if(!serial_can_read(serial_fd,timeout)){
		// read timeout
		return 0;
	}

	rc = read(serial_fd,buf,buf_len);

	return rc;
}

static int init(const char* serialPath){
	if(serialPath == NULL){
		serialPath = defaultSerialPath;
	}
	serial_fd = serial_port_open(serialPath,38400);
	if(serial_fd < 0){
		ERROR("Open serial port %s failed",serialPath);
		return -1;
	}
	//serial_port_set_nonblock(serialFd,0);
	return 0;
}

static void read_config(char* serialPath) {
	if(init(serialPath) < 0){
		return;
	}

	// Send the read_config command 0x0C
	int bytes = 0;
	uint8_t payload[] = {
			0xC0, // TFEND
			0x0C, // port 0x00, cmd 0x0C
			0x00, // null body
			0xC0  // TFEND
	};
	bytes = write_to_serial(payload,4,15);
	if(bytes != 4){
		ERROR("Send command failed, write error: %d",errno);
		return;
	}

	// Read the tnc response
	uint8_t cfg_encoded[4096];
	memset(cfg_encoded, 0, sizeof(cfg_encoded));
	bytes = read_from_serial(cfg_encoded,sizeof(cfg_encoded),15);
	if(bytes <= 0){
		ERROR("Read config failed, error: %d",errno);
		return;
	}

	// decode
	uint8_t out[8192];
	uint16_t out_len = sizeof(out);
	memset(out, 0, out_len);
	uint8_t ch = 0, cmd = 0;
	// trying to decode
	if(kiss_decode_ex(cfg_encoded, bytes, &ch, &cmd, out, &out_len) <= 0){
		ERROR("Decode config data filed, in: %d bytes, out: %d bytes",bytes,out_len);
		return;
	}
	// decode success!
	DBG("decoded: %*s", out_len, out);
	log_hexdump(out,out_len);

	if(cmd != 0x0c){
		// It's the correct config data!
		WARN("Invalid response type: 0x0%x, expected 0x0C",cmd);
	}

	// convert and save to local config file ./tnc.cfg


}

static void write_config(char* fileName) {
//	uint8_t out[128];
//	memset(out, 0, sizeof(out));
//	uint16_t out_len = 128;
//	kiss_encode_ex(test_data_for_enc, sizeof(test_data_for_enc), 0, 0, out, &out_len);
//	printf("encoded \n");
//	hexdump(out,out_len);
	printf("todo - write config\n");
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		_usage(argc, argv);
	} else {
		if (strncmp("read", argv[1], 4) == 0) {
			if(argc > 2){
				read_config(argv[2]);
			}else{
				read_config(NULL);
			}
		} else if (strncmp("write", argv[1], 5) == 0) {
			if(argc > 2){
				write_config(argv[2]);
			}else{
				write_config(NULL);
			}
		}
	}
	return 0;
}
