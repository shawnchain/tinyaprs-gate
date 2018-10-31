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
#include "serial.h"

#define KISS_FEND  0xc0
#define KISS_FESC  0xdb
#define KISS_TFEND 0xdc
#define KISS_TFESC 0xdd

enum {
	KISS_CMD_DATA = 0,
	KISS_CMD_TXDELAY,
	KISS_CMD_CONFIG_TEXT = 0x0B,
	KISS_CMD_CONFIG_CALL = 0x0C,
	KISS_CMD_CONFIG_PARAMS = 0x0D,
	KISS_CMD_CONFIG_ERROR = 0x0E,
	KISS_CMD_CONFIG_MAGIC = 0x0F,
	KISS_CMD_Return = 0xFF
};


#include "tnc_settings_types.h"

typedef struct{
	uint8_t port;
	uint8_t cmd;
	uint8_t data[5120];   // kiss frame data
	uint16_t dataLen;	   // kiss frame length
}KissFrame;

static int kiss_decode_ex(KissFrame *kissFrame, uint8_t *payloadOut, uint16_t *payloadOutLen) {
	// byte 0 shold be 0xC0
	if (kissFrame->dataLen < 3 || kissFrame->data[0] != KISS_FEND) {
		return -1;
	}

	uint16_t i = 0, oLen = 0;
	bool escaped = false;

	DBG("KISS frame buffer: sizeof((*kissFrame).data) = %d",sizeof((*kissFrame).data));

	// byte 1 should be the channels
	kissFrame->port = kissFrame->data[1] >> 4 & 0x0f;
	kissFrame->cmd = kissFrame->data[1] & 0x0f;
	i = 2;
	uint8_t *in = kissFrame->data;
	uint8_t *out = payloadOut;

	for (; i < kissFrame->dataLen; i++) {
		uint8_t c = in[i];
		if (c == KISS_FEND) {
			// end flag
			break;
		} else if (c == KISS_FESC) {
			escaped = true;
		} else if (c == KISS_TFESC && escaped) {
			out[oLen++] = KISS_FESC;
			escaped = false;
		} else if (c == KISS_TFEND && escaped) {
			out[oLen++] = KISS_FEND;
			escaped = false;
		} else {
			out[oLen++] = c & 0xff;
			escaped = false; // reset the escape flag
		}
	}

	DBG("Decoded %d bytes of KISS frame data, port: 0x%0x, cmd: 0x%0x", oLen, kissFrame->port,kissFrame->cmd);
	*payloadOutLen = oLen;
	return oLen;
}

static void kiss_encode_begin(KissFrame *frame,uint8_t port, uint8_t cmd){
	frame->port = port;
	frame->cmd = cmd;
	frame->data[0] = KISS_FEND;
	frame->data[1] = (frame->port << 4 | frame->cmd) ; // ch 0, cmd 0;
	frame->dataLen = 2;
}

static void kiss_encode_append(KissFrame *frame,uint8_t *payloadIn, uint16_t payloadInLen){
	uint16_t i = 0, o = frame->dataLen;
	uint8_t *out = frame->data;
	for (i = 0; i < payloadInLen && o < sizeof((*frame).data) - 3; i++) {
		uint8_t c = payloadIn[i];
		switch (c) {
		case KISS_FEND:
			out[o++] = KISS_FESC;
			out[o++] = KISS_TFEND;
			break;
		case KISS_FESC:
			out[o++] = KISS_FESC;
			out[o++] = KISS_TFESC;
			break;
		default:
			out[o++] = c;
		}
	}
	frame->dataLen = o;
	return;
}

static void kiss_encode_end(KissFrame *frame){
	frame->data[frame->dataLen] = KISS_FEND;
	frame->dataLen++;
}

static int kiss_encode_ex(KissFrame *frame,uint8_t port, uint8_t cmd, uint8_t *payloadIn, uint16_t payloadInLen){
	kiss_encode_begin(frame, port,cmd);
	kiss_encode_append(frame, payloadIn, payloadInLen);
	kiss_encode_end(frame);
	return frame->dataLen;
}

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
	if(fd < 0) return false;
	fd_set rset;
	struct timeval timeo;
	int rc;

	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	timeo.tv_sec = timeout;
	timeo.tv_usec = 0;
	rc = select(fd + 1, &rset, NULL, NULL, &timeo);
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
	int i = 0;
	uint8_t c = 0;
	do{
		 while(i < buf_len && read(serial_fd,&c,1) > 0 ){
			//DBG("0x%02x ",c);
			buf[i++] = c;
		 };
	}while(i == 0 && serial_can_read(serial_fd,timeout));
	if(i > 0)
		DBG("Read %d bytes",i);
	else
		INFO("Read time out");
	return i;
}

static int init(const char* serialPath){
	if(serialPath == NULL){
		serialPath = defaultSerialPath;
	}
	serial_fd = serial_open(serialPath,115200);
	if(serial_fd < 0){
		ERROR("Open serial port %s failed",serialPath);
		return -1;
	}
	set_nonblock(serial_fd,true);
	return 0;
}

static uint8_t calc_crc(uint8_t *data, uint16_t len){
	uint8_t crc = 0;
	uint16_t i = 0;
	for(;i<len;i++){
		crc += data[i];
	}
	return ~crc;
}

static bool _read_config_response(uint8_t *dataOut, uint16_t *dataOutLen, bool checkcrc){
	// Read the tnc response
	KissFrame frame;
	memset(&frame, 0, sizeof(KissFrame));
	uint16_t bytes = read_from_serial((uint8_t*)&frame.data,sizeof(frame.data),2);
	if(bytes <= 0){
		// read failed
		*dataOutLen = 0;
		return false;
	}
	frame.dataLen = bytes;

	// decode kiss frame
	if(kiss_decode_ex(&frame,dataOut,dataOutLen) < 0){
		ERROR("Invalid response(%d bytes), please check your TNC.",bytes);
		log_hexdump(frame.data,bytes);
		return false;
	}

	// calculate the CRC of config payload
	if(*dataOutLen == 0){
		INFO("empty frame");
		return false;
	}

	uint8_t len = *dataOutLen;
	if(checkcrc){
		len = *dataOutLen -1;
		uint8_t crc = calc_crc(dataOut,len);
		if(crc != dataOut[len]){
			// crc check failed
			INFO("CRC Check failed, payload dump:");
			log_hexdump(dataOut,*dataOutLen);
			*dataOutLen = 0;
			return false;
		}else{
			DBG("CRC Check OK");
		}
	}

	// decode success!
	if(len > 0){
		DBG("payload dump: ");
		log_hexdump(dataOut,len /* not including the CRC*/);
	}else{
		DBG("empty payload, length = 0");
	}
	//TODO handle the configuration frame ?
	*dataOutLen = len;
	return true;
}

/*
 * consturct the READ config payload and send to tnc
 */
static bool _read_config(uint8_t cmd, uint8_t *dataOut, uint16_t *dataOutLen,bool checkcrc){
	KissFrame req;
	uint8_t data = 0xff;
	kiss_encode_ex(&req,0,cmd,&data,1);

	int bytes = 0;
	bytes = write_to_serial(req.data,req.dataLen,2);
	if(bytes != req.dataLen){
		ERROR("Send command failed, write error: %d",errno);
		return false;
	}

	return _read_config_response(dataOut, dataOutLen, checkcrc);
}

/*
 * Construct the WRITE config payload and send to tnc
 */
#if 0
static bool _write_config(uint8_t cmd, uint8_t *data, uint16_t dataLen, bool needCrc, bool needResponse){
	KissFrame req;
	kiss_encode_begin(&req,0,cmd);
	kiss_encode_append(&req,data,dataLen);

	if(needCrc){
		// add crc byte
		uint8_t crc = calc_crc(data,dataLen);
		kiss_encode_append(&req,&crc,1);
	}
	kiss_encode_end(&req);

	int bytes = write_to_serial(req.data,req.dataLen,2);
	if(bytes != req.dataLen){
		ERROR("Send command failed, write error: %d",errno);
		return false;
	}else{
		DBG("Send command success");
	}

	if(needResponse){
		// Read the tnc response
		uint8_t out[64];
		uint16_t outLen = sizeof(out);
		if(_read_config_response(out,&outLen,needCrc)){
			// dump the response
			if(outLen > 0){
				// TODO - verify the tnc respond
			}else{
				DBG("TNC responsed NO error");
			}
		}else{
			INFO("No response for the write command");
			return false;
		}
	}
	return true;
}
#endif

#define TO_LITTLE_ENDIAN_16(x) x

#pragma pack(1)
typedef struct {
	SettingsData settings;
	CallData calldata;
	char beaconText[256];
}TNCData;
static TNCData tncData;
static void dump_config(){
	DBG("==========================================================");
	DBG("==TNC Configurations                                    ==");
	DBG("==========================================================");
	DBG("Parameters: ");
	DBG("  Run mode: %d",tncData.settings.run_mode);
	DBG("  Beacon symbol: %c%c",tncData.settings.beacon.symbol[0],tncData.settings.beacon.symbol[1]);
	DBG("  Beacon interval: %d",TO_LITTLE_ENDIAN_16(tncData.settings.beacon.interval)); // FIXME byte-order should be little endian

	DBG("  RF tx delay: %d",tncData.settings.rf.txdelay);
	DBG("  RF tx tail: %d",tncData.settings.rf.txtail);
	DBG("  RF persistence: %d",tncData.settings.rf.persistence);
	DBG("  RF slot time: %d",tncData.settings.rf.slot_time);
	DBG("  RF duplex mode: %d",tncData.settings.rf.duplex);

	DBG("CallSigns: ");
	DBG("  My Call: %6s-%d",tncData.calldata.myCall.call,tncData.calldata.myCall.ssid);
	DBG("  Dest Call: %6s-%d",tncData.calldata.destCall.call,tncData.calldata.destCall.ssid);
	DBG("  Path1: %6s-%d",tncData.calldata.path1.call,tncData.calldata.path1.ssid);
	DBG("  Path2: %6s-%d",tncData.calldata.path2.call,tncData.calldata.path2.ssid);
}

static void read_config(char* serialPath) {
	if(init(serialPath) < 0){
		return;
	}
	memset(&tncData,0,sizeof(TNCData));

	// Reading Paramerts
	INFO("Reading TNC settings...");
	uint8_t resp[256];
	uint16_t respLen = sizeof(resp);
	if(!_read_config(KISS_CMD_CONFIG_PARAMS,resp,&respLen,true)){
		DBG("Read TNC params failed");
		return;
	}
	if(sizeof(SettingsData) != respLen){
		INFO("TNC params length mismatch! Expected %d, got %d",sizeof(SettingsData),respLen);
		return;
	}
	memcpy((uint8_t*)&tncData.settings, resp,respLen);
	DBG("TNC params read OK");


	// Reading CallData
	memset(resp,0,sizeof(resp));
	respLen = sizeof(resp);
	if(!_read_config(0x0C,resp,&respLen,false)){
		DBG("Read TNC calldata failed");
		return;
	}
	if(sizeof(CallData) != respLen){
		INFO("TNC calldata length mismatch! Expected %d, got %d",sizeof(CallData),respLen);
		return;
	}
	memcpy((uint8_t*)&tncData.calldata, resp,respLen);
	DBG("TNC calldata read OK");

	dump_config();
	// convert and save to local config file ./tnc.cfg

#if 0
	// TEST WRITE
	INFO("Updating tnc params...");
	tncData.settings.beacon.interval = 3600;
	tncData.settings.run_mode = 0;
	uint8_t *data = (uint8_t*)&tncData.settings;
	uint16_t len = sizeof(SettingsData);
	if(_write_config(KISS_CMD_CONFIG_PARAMS,data,len,true,true)){
		INFO("Update tnc params success");
	}else{
		ERROR("Update tnc params, write error: %d",errno);
	}

	INFO("Commiting tnc params...");
	uint8_t commit[4] = {
			0x0c,0x01,0x01,0x07
	};
	if(_write_config(KISS_CMD_CONFIG_MAGIC,commit,4,true,true)){
		INFO("Commit tnc params success");
	}else{
		ERROR("Commit tnc params, write error: %d",errno);
	}

	INFO("Rebooting tnc ...");
	uint8_t reboot[4] = {
			0x0B,0x00,0x00,0x07
	};
	_write_config(KISS_CMD_CONFIG_MAGIC,reboot,4,true,true);
#endif


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
