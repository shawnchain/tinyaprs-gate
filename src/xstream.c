#include "xstream.h"
#include "log.h"

#include <assert.h>
#include <string.h>

#define DEBUG_XSTREAM 0

/////////////////////////////////////////////////////////////////////////////////////////////////////
// ustream callbacks

static void on_ustream_notify_read(struct ustream *s, int bytes){
	struct ustream_fd *_sfd = container_of(s, struct ustream_fd, stream);
	struct xstream *x = container_of(_sfd, struct xstream, stream_fd);

	assert(x->_codec.decode_func);
	struct xstream_ctx *ctx = &x->_ctx;

	unsigned int bytes_total = s->r.data_bytes;
	unsigned int bytes_decoded = 0;

	while(bytes_decoded < bytes_total) {
		// get available data bytes from ustream
		int len = 0;
		char *data = ustream_get_read_buf(&x->stream_fd.stream, &len);

		if (!data || len == 0) {
			// nothing to read
			break;
		}

		//TODO - supports more codecs
		ctx->currentCodec = &x->_codec;

		// calling codec
		// the "len" will be changed inside codec implementation indiecates how many bytes are consumed.
		if (x->_codec.decode_func(ctx, data, &len)){
			// check the result if decode success
			assert(ctx->currentObj);
			if(x->on_read_cb) {
				x->on_read_cb(x, ctx->currentObj->data, ctx->currentObj->dataLen);
			}

			// we're done, reset states
			ctx->currentObj = NULL;
		}

		// check how many bytes are consumed
		if (len == 0){
			// do nothing, bail out the loop and 
			// keep data untouched
			break;
		}

		// update for next iteration
		ustream_consume(s, len);
		bytes_decoded += len;
	}

	#if DEBUG_XSTREAM
	DBG("total %d bytes decoded", bytes_decoded);
	#endif
}

static void on_ustream_notify_write(struct ustream* s, int bytes){
	struct ustream_fd *_sfd = container_of(s, struct ustream_fd, stream);
	struct xstream *x = container_of(_sfd, struct xstream, stream_fd);
	if (x->on_write_cb){
		x->on_write_cb(x,bytes);
	}
}

static void on_ustream_notify_state(struct ustream* s){
	struct ustream_fd *_sfd = container_of(s, struct ustream_fd, stream);
	struct xstream *x = container_of(_sfd, struct xstream, stream_fd);
	if (s->eof || s->write_error){
		if (x->on_error_cb) {
			x->on_error_cb(x);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// xstream codec implementation
// 2 types of codec:
//  - default codec that by-pass all data underlying buffer to the delegate.
//  - crlf codec that parse each byte and notify delegate when CR/LF is met.

static struct xstream_obj objbuf = {
	.type = "XOBJ",
	.data = NULL,
	.dataLen = 0,
};

/*
 * default decode function that pass through all received data in stream buffer
 */
static bool xstream_decode_default(struct xstream_ctx *ctx, char *data, int *len){
	assert(ctx);
	assert(data);
	assert(len);

	if (!ctx->currentObj) {
		ctx->currentObj = &objbuf;
	}

	ctx->currentObj->data = data;
	ctx->currentObj->dataLen = *len;

	return true;
}

/*
 * This is a zero copy version of line reader.
 * It will parse each bytes in buffer and consume them all when a \n or \r is met.
 * if CRLF is not found, the data is kept in buffer and get untouched (no bytes consumed).
 * the ctx.bytesRead keeps the offset from byte buffer for pending line data before CRLF is met.
 */
static bool xstream_decode_crlf(struct xstream_ctx *ctx, char *data, int *len){
	if (!ctx->currentObj) {
		ctx->currentObj = &objbuf;
		ctx->currentObj->dataLen = 0;
	}

	struct xstream_obj *obj = ctx->currentObj;
	struct xstream_codec *codec = ctx->currentCodec;

	// we store the read offset in current_obj->dataLen
	while(obj->dataLen < *len /*all bytes available in ustream read buffer*/ ){
		if (data[obj->dataLen] != '\n' && data[obj->dataLen] != '\r') {
			obj->dataLen++;
			continue;
		}

		obj->data = data;
		*len = obj->dataLen + 1; // bytes to be consumed from ustream buffer, including the CR/LF char.

		if (codec && codec->codec_opts & XSTREAM_KEEP_CRLF) {
			obj->dataLen++;  // keeps the CRLF char in returning data if required.
		}

		#if DEBUG_XSTREAM
		DBG("decoded %d bytes", *len);
		#endif

		return true;
	}

	*len = 0; // consume 0 byte, keep data in ustream buffer

	// TODO - timeout check
	return false;
}

struct xstream_codec _codec_obj_crlf = {
		.name = "crlf",
		.decode_func = xstream_decode_crlf,
		.codec_opts = XSTREAM_KEEP_CRLF
};

struct xstream_codec _codec_obj_default = {
		.name = "default",
		.decode_func = xstream_decode_default,
		.codec_opts = 0
};

const struct xstream_codec* xstream_codec_crlf(){
	return &_codec_obj_crlf;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
// xstream implementation

void xstream_crlf_init(struct xstream *x, int fd, int init_opts, xstream_read_callback on_read, xstream_write_callback on_write, xstream_error_callback on_error){
	xstream_init(x,fd,on_read, on_write, on_error);
	x->_codec.decode_func = xstream_decode_crlf;
	x->_codec.codec_opts |= init_opts;
}

void xstream_init(struct xstream *x, int fd, xstream_read_callback on_read, xstream_write_callback on_write, xstream_error_callback on_error){
	memset(x,0,sizeof(*x));
	x->stream_fd.stream.notify_read  = on_ustream_notify_read;
	x->stream_fd.stream.notify_write = on_ustream_notify_write;
	x->stream_fd.stream.notify_state = on_ustream_notify_state;
	ustream_fd_init(&x->stream_fd, fd);
	
	x->on_read_cb = on_read;
	x->on_write_cb = on_write;
	x->on_error_cb = on_error;
	x->_codec.decode_func = xstream_decode_default;

	// setup the associated singleton ctx
	x->_ctx.stream = x;
}

void xstream_set_codec(struct xstream *x, const struct xstream_codec *codec){
	memcpy(&x->_codec,codec,sizeof(struct xstream_codec));
}

void xstream_free(struct xstream *x){
    if (x->stream_fd.fd.registered){
        ustream_free(&x->stream_fd.stream);
    }
}

