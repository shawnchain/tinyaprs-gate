/*
 * kiss.h
 *
 *  Created on: 2016年9月3日
 *      Author: shawn
 */

#ifndef SRC_KISS_H_
#define SRC_KISS_H_

#include "iokit.h"

//typedef struct KissReader {
//	//uint8_t buf[ CONFIG_AX25_FRAME_BUF_LEN ];
//	uint8_t *buf;
//	uint16_t bufLen;
//	size_t pos;            // next byte to fill
//	time_t last_tick;      // timestamp of last byte into buf
//}KissReader;

//typedef struct KissReader{
//	struct IOReader reader;
//}KissReader;
#define KissReader IOReader

typedef io_read_callback kiss_decode_callback;

void kiss_init(struct KissReader *k, int fd, uint8_t *buf, uint16_t bufLen, kiss_decode_callback callback);
//void kiss_queue_message(uint8_t *buf, size_t len);
//void kiss_queue_process(void);
//void kiss_encode(uint8_t port, uint8_t *buf, size_t len);

#endif /* SRC_KISS_H_ */
