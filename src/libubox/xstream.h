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

/*
 * The xstream object
 */
struct xstream_obj {
	char			type[4];
	char			*data;
	unsigned int 	dataLen;
	unsigned int 	dataSizeMax;
};

/*
 * The xstream context
 */
struct xstream_ctx {
	struct xstream  	 *stream;

	struct xstream_codec *currentCodec;
	struct xstream_obj	 *currentObj;
};

/*
 * The xstream codec object
 */
typedef bool (*xstream_decode_func)(struct xstream_ctx *ctx, char *data, int *len);
struct xstream_codec {
	char 	name[8];
	bool 	(*decode_func)(struct xstream_ctx *ctx, char *data, int *len);
	int 	codec_opts;
};

/*
 * The xstream callbacks
 */
struct xstream;
typedef void (*xstream_read_callback) (struct xstream *x, char *bytes, int len);
typedef void (*xstream_write_callback)(struct xstream *x, int len);
typedef void (*xstream_error_callback)(struct xstream *x);

/*
 * The xstream 
 */
struct xstream {
	struct ustream_fd  		stream_fd;

	// internal parts
	xstream_read_callback   on_read_cb;
	xstream_write_callback  on_write_cb;
	xstream_error_callback  on_error_cb;

	// other stateful objects, currently 1:1 mapped to stream
	struct xstream_ctx 		_ctx;
	struct xstream_codec	_codec;
};

/*
 * Initialize the xstream with crlf codec configured.
 */
void xstream_crlf_init(struct xstream *x, int fd, int init_opts, xstream_read_callback on_read, xstream_write_callback on_write, xstream_error_callback on_error);

/*
 * Initialize the xstream with default pass-through codec configured.
 */
void xstream_init(struct xstream *x, int fd, xstream_read_callback on_read, xstream_write_callback on_write, xstream_error_callback on_error);

/*
 * Free xstream resources and detach from the uloop
 */
void xstream_free(struct xstream *x);

/*
 * Get the crlf codec object
 */
const struct xstream_codec* xstream_codec_crlf();

/*
 * Set codec of an xstream
 */
void xstream_set_codec(struct xstream *x, const struct xstream_codec *codec);

int xstream_write(struct xstream *x, char* bytes, int len);

#endif