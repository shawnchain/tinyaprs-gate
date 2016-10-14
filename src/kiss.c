/*
 * kiss.c
 *
 *  Created on: 2016年9月3日
 *      Author: shawn
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

#include "kiss.h"

#define KISS_QUEUE	0

#define KISS_FEND  0xc0
#define KISS_FESC  0xdb
#define KISS_TFEND 0xdc
#define KISS_TFESC 0xdd

enum {
	KISS_CMD_DATA = 0,
	KISS_CMD_TXDELAY,
	KISS_CMD_P,
	KISS_CMD_SlotTime,
	KISS_CMD_TXtail,
	KISS_CMD_FullDuplex,
	KISS_CMD_SetHardware,
	KISS_CMD_Return = 0xff
};

enum {
	KISS_DUPLEX_HALF = 0,
	KISS_DUPLEX_FULL
};

enum {
	KISS_QUEUE_IDLE,
	KISS_QUEUE_DELAYED,
};


#if KISS_QUEUE > 0
time_t kiss_queue_ts;
uint8_t kiss_queue_state;
size_t kiss_queue_len = 0;
struct KissReader kiss_queue[KISS_QUEUE];
#endif

uint8_t kiss_txdelay;
uint8_t kiss_txtail;
uint8_t kiss_persistence;
uint8_t kiss_slot_time;
uint8_t kiss_duplex;

static void kiss_decode_putc(struct KissReader *k, unsigned char c);
static void kiss_decode_frame(struct KissReader *k);
//static void kiss_encode(struct KissReader *k, uint8_t port, uint8_t *buf, size_t len);

//static struct KissReader k = {.bufferLen = 0};

static int kiss_read(struct KissReader *reader){
	// read the whole data from reader and pass to the kiss decoder
	if(reader->fd < 0) return -1;
	int rc = 0,bytesRead = 0;
	uint8_t c = 0;
	while((rc = read(reader->fd,&c,1)) >0){
		kiss_decode_putc(reader,c);
		bytesRead++;
//
//		if(c == '\r' || c == '\n'){
//			flush = true;
//		}else{
//			flush = false;
//			reader->buffer[reader->bufferLen] = c;
//			reader->bufferLen++;
//			if(reader->bufferLen == (reader->maxBufferLen - 1)){
//				DBG("read buffer full!");
//				// we're full!
//				reader->buffer[reader->bufferLen] = 0;
//				reader->bufferLen--; // not including the \0
//				flush = true;
//			}
//		}
//		if(flush && reader->bufferLen > 0 && reader->callback ){
//			reader->buffer[reader->bufferLen] = 0;
//			reader->callback(reader->buffer,reader->bufferLen);
//			reader->bufferLen = 0;
//			flush = false;
//		}
	}

	if(rc < 0){
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			rc = 0;
		}else{
			ERROR("*** io_readkiss read() error %d: %s", rc , strerror(errno));
		}
	}

	if(bytesRead > 0){
		rc = bytesRead;
	}
	return rc;
}

static int kiss_flush(struct KissReader *k){
	// do nothing
	k->bufferLen = 0;
	return 0;
}

static int kiss_run(struct KissReader *k){
	if ((k->bufferLen > 0) && (get_time_milli_seconds() - k->lastRead  >  2000)) {
		INFO("KISS read - Timeout");
		k->bufferLen = 0;
	}
	return 0;
}

static int kiss_close(struct KissReader *k){
	if(k->fd >=0){
		close(k->fd);
	}
	bzero(k,sizeof(struct IOReader));
	k->fd = -1;
	return 0;
}

void kiss_init(struct KissReader *k, int fd, uint8_t *buf, uint16_t bufLen, kiss_decode_callback callback)
{
	kiss_txdelay = 50;
	kiss_persistence = 63;
	kiss_txtail = 5;
	kiss_slot_time = 10;
	kiss_duplex = KISS_DUPLEX_HALF;

	bzero(k,sizeof(struct KissReader));

	k->fd = fd;
	k->buffer = buf;			// Shared buffer
	k->maxBufferLen = bufLen; // buffer length, should be >= CONFIG_AX25_FRAME_BUF_LEN
	k->fnRead = kiss_read;
	k->fnFlush = kiss_flush;
	k->fnRun = kiss_run;
	k->fnClose = kiss_close;
	k->callback = callback;

	DBG("KISS reader initialized.");
}

static void kiss_decode_putc(struct KissReader *k, uint8_t c){
	static bool escaped = false;
	// sanity checks
	// no serial input in last 2 secs?
	// moved to k->fnRun()
#if 0
	if ((k->bufferLen != 0) && (get_time_milli_seconds() - k->lastRead  >  2000)) {
		INFO("KISS read - Timeout");
		k->bufferLen = 0;
	}
#endif

	// about to overflow buffer? reset
	if (k->bufferLen >= (k->maxBufferLen - 2)) {
		INFO("KISS - Packet too long %d >= %d", k->bufferLen, k->maxBufferLen - 2);
		k->bufferLen = 0;
	}

	if (c == KISS_FEND) {
		if ((!escaped) && (k->bufferLen > 0)) {
			kiss_decode_frame(k);
		}
		k->bufferLen = 0;
		escaped = false;
		return;
	} else if (c == KISS_FESC) {
		escaped = true;
		return;
	} else if (c == KISS_TFESC) {
		if (escaped) {
			escaped = false;
			c = KISS_FESC;
		}
	} else if (c == KISS_TFEND) {
		if (escaped) {
			escaped = false;
			c = KISS_FEND;
		}
	} else if (escaped) {
		escaped = false;
	}

	k->buffer[k->bufferLen] = c & 0xff;
	k->bufferLen++;
	k->lastRead = get_time_milli_seconds();
}

static void kiss_decode_frame(struct KissReader *k){
	uint8_t cmd;
	uint8_t port;

	// First check exit frame
	if(k->bufferLen == 1 && k->buffer[0] == KISS_CMD_Return){
		INFO("KISS - got 1 byte quit frame");
		if(k->callback){
			k->callback(k->buffer,k->bufferLen);
		}
		return;
	}

	// the first byte of KISS message is for command and port
	cmd = k->buffer[0] & 0x0f;
	port = k->buffer[0] >> 4;
	if (port > 0) {
		// not supported yet.
		WARN("KISS frame with port > 0 [%d] is not supported yet",port);
		return;
	}
	if (k->bufferLen < 2) {
		INFO("KISS - discard invalid frame, too short");
		return;
	}

	switch(cmd){
		case KISS_CMD_DATA:{
			DBG("KISS - got data frame %d bytes",k->bufferLen - 1);
			if(k->callback){
				k->callback(k->buffer +1,k->bufferLen -1);
			}
			break;
		}
//		case KISS_CMD_TXDELAY:{
//			INFO("KISS - setting txdelay %d\n", k->buffer[1]);
//			if(k->buffer[1] > 0){
//				kiss_txdelay = k->buffer[1];
//			}
//			break;
//		}
//		case KISS_CMD_P:{
//			INFO("Kiss - setting persistence %d\n", k->buffer[1]);
//			if(k->buffer[1] > 0){
//				kiss_persistence = k->buffer[1];
//			}
//			break;
//		}
//		case KISS_CMD_SlotTime:{
//			INFO("Kiss - setting slot_time %d\n", k->buffer[1]);
//			if(k->buffer[1] > 0){
//				kiss_slot_time = k->buffer[1];
//			}
//			break;
//		}
//		case KISS_CMD_TXtail:{
//			INFO("Kiss - setting txtail %d\n", k->buffer[1]);
//			if(k->buffer[1] > 0){
//				kiss_txtail = k->buffer[1];
//			}
//			break;
//		}
//		case KISS_CMD_FullDuplex:{
//			INFO("Kiss - setting duplex %d\n", k->buffer[1]);
//			kiss_duplex = k->buffer[1];
//			break;
//		}
		default:
			DBG("Unsupported KISS cmd type: %d.",cmd);
			break;
	}// end of switch(cmd)
}

//static void kiss_encode(struct KissReader *k, uint8_t port, uint8_t *buf, size_t len){
//	size_t i;
//
//	kfile_putc(KISS_FEND, k.fd);
//	kfile_putc((port << 4) & 0xf0, k.fd);
//
//	for (i = 0; i < len; i++) {
//
//		uint8_t c = buf[i];
//
//		if (c == KISS_FEND) {
//			kfile_putc(KISS_FESC, k.fd);
//			kfile_putc(KISS_TFEND, k.fd);
//			continue;
//		}
//
//		kfile_putc(c, k.fd);
//
//		if (c == KISS_FESC) {
//			kfile_putc(KISS_TFESC, k.fd);
//		}
//	}
//
//	kfile_putc(KISS_FEND, k.fd);
//}

//void kiss_queue_message(/*channel = 0*/ uint8_t *buf, size_t len){
//#if KISS_QUEUE > 0
//	if (kiss_queue_len == KISS_QUEUE)
//		return;
//
//	memcpy(kiss_queue[kiss_queue_len].buf, buf, len);
//	kiss_queue[kiss_queue_len].pos = len;
//	kiss_queue_len ++;
//#else
//	INFO("Kiss - queue disabled, sending message\n");
//	//ax25_sendRaw(&g_ax25, buf, len);
//#endif
//}
//
//void kiss_queue_process()
//{
//#if KISS_QUEUE > 0
//	uint8_t random;
//
//	if (kiss_queue_len == 0) {
//		return;
//	}
///*
//	if (kiss_afsk->cd) {
//		return;
//	}
//*/
//	if (kiss_ax25->dcd) {
//		return;
//	}
//
//	if (kiss_queue_state == KISS_QUEUE_DELAYED) {
//		if (timer_clock() - kiss_queue_ts <= ms_to_ticks(kiss_slot_time * 10)) {
//			return;
//		}
//		INFO("Queue released\n");
//	}
//
//	random = (uint32_t)rand() & 0xff;
//	INFO("Queue random is %d\n", random);
//	if (random > kiss_persistence) {
//		INFO("Queue delayed for %dms\n", kiss_slot_time * 10);
//
//		kiss_queue_state = KISS_QUEUE_DELAYED;
//		kiss_queue_ts = timer_clock();
//
//		return;
//	}
//
//	INFO("Queue sending packets: %d\n", kiss_queue_len);
//	for (size_t i = 0; i < kiss_queue_len; i++) {
//		ax25_sendRaw(kiss_ax25, kiss_queue[i].buf, kiss_queue[i].pos);
//	}
//
//	kiss_queue_len = 0;
//	kiss_queue_state = KISS_QUEUE_IDLE;
//#endif
//}
