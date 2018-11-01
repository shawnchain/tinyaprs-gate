/*
 * xtream.h
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#ifndef XSTREAM_H_
#define XSTREAM_H_

#include <libubox/ustream.h>

#define XSTREAM_KEEP_CRLF  1

// xstream callbacks
struct xstream;
typedef void (*xstream_read_callback) (struct xstream *x, char *bytes, int len);
typedef void (*xstream_write_callback)(struct xstream *x, int len);
typedef void (*xstream_error_callback)(struct xstream *x);

// xstream codec implementation
struct xstream_ctx;
typedef bool (*xstream_decode_func)(struct xstream_ctx *ctx, struct ustream *s, unsigned int opt);

struct xstream_ctx {
	char			*data;
	unsigned int    dataLen;
	
	char 			decodeBuf[1024];
	unsigned int 	decodeBufLen;
	unsigned int 	decodeBufOffset;

	unsigned int    bytesRead;
	unsigned int    bytesConsumed;

	void			*obj;
};

struct xstream {
	struct ustream_fd  stream_fd;
	xstream_read_callback 	on_read_cb;
	xstream_write_callback 	on_write_cb;
	xstream_error_callback	on_error_cb;

	struct xstream_ctx 		ctx;
	xstream_decode_func 	decode_func;
	unsigned int 			decode_opts;
};

void xstream_crlf_init(struct xstream *x, int fd, int init_opts, xstream_read_callback on_read, xstream_write_callback on_write, xstream_error_callback on_error);

void xstream_init(struct xstream *x, int fd, xstream_read_callback on_read, xstream_write_callback on_write, xstream_error_callback on_error);

void xstream_set_decode_func(struct xstream *x, xstream_decode_func decode_func);

void xstream_free(struct xstream *x);

bool xstream_decode_crlf(struct xstream_ctx *ctx, struct ustream *s, unsigned int opt);

#endif