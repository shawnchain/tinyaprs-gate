#include "xstream.h"
#include "log.h"

#include <assert.h>
#include <string.h>

#define DEBUG_XSTREAM 0

static void ustream_notify_read(struct ustream *s, int bytes){
	struct ustream_fd *_sfd = container_of(s, struct ustream_fd, stream);
	struct xstream *x = container_of(_sfd, struct xstream, stream_fd);

	assert(x->decode_func);

	unsigned int bytes_decoded = 0;
	unsigned int bytes_total = s->r.data_bytes;

	while(bytes_decoded < bytes_total) {
		if (x->decode_func(&x->ctx, &x->stream_fd.stream, x->decode_opts)){
			
			if(x->on_read_cb) {
				x->on_read_cb(x, x->ctx.data, x->ctx.dataLen);
			}

			// we're done
			struct xstream_ctx *ctx = &x->ctx;
			ctx->bytesRead = 0;
			ctx->decodeBufOffset = 0;
			ctx->dataLen = 0;
		}

		if (x->ctx.bytesConsumed == 0){
			// do nothing, bail out the loop and 
			// keep data untouched
			break;
		}

		ustream_consume(s,x->ctx.bytesConsumed);
		bytes_decoded += x->ctx.bytesConsumed;
		x->ctx.bytesConsumed = 0;
	}
	#if DEBUG_XSTREAM
	DBG("total %d bytes decoded", bytes_decoded);
	#endif
}

static void ustream_notify_write(struct ustream* s, int bytes){
	struct ustream_fd *_sfd = container_of(s, struct ustream_fd, stream);
	struct xstream *x = container_of(_sfd, struct xstream, stream_fd);
	if (x->on_write_cb){
		x->on_write_cb(x,bytes);
	}
}

static void ustream_notify_state(struct ustream* s){
	struct ustream_fd *_sfd = container_of(s, struct ustream_fd, stream);
	struct xstream *x = container_of(_sfd, struct xstream, stream_fd);
	if (s->eof || s->write_error){
		if (x->on_error_cb) {
			x->on_error_cb(x);
		}
	}
}

/* default decode function that pass through all received data in stream buffer */
static bool xstream_decode_default(struct xstream_ctx *ctx, struct ustream *s, unsigned int opt){
	int len = 0;
	char *data = ustream_get_read_buf(s, &len);

	ctx->data = data;
	ctx->dataLen = len;
	ctx->bytesRead = len;
	ctx->bytesConsumed = len;

	return true;
}

/*
 * This is a zero copy version of line reader.
 * It will parse each bytes in buffer and consume them all when a \n or \r is met.
 * if CRLF is not found, the data is kept in buffer and get untouched (no bytes consumed).
 * the ctx.bytesRead keeps the offset from byte buffer for pending line data before CRLF is met.
 */
bool xstream_decode_crlf(struct xstream_ctx *ctx, struct ustream *s, unsigned int opt){
	int len = 0;
	char *data = ustream_get_read_buf(s, &len);

	while(ctx->bytesRead < len /*all bytes available in ustream read buffer*/){
		if (data[ctx->bytesRead] == '\n' || data[ctx->bytesRead] == '\r') {
			ctx->data = data;
			ctx->dataLen = ctx->bytesRead;
			if (opt & XSTREAM_KEEP_CRLF) {
				ctx->dataLen++;
			}
			ctx->bytesConsumed = ctx->bytesRead + 1;
			ctx->bytesRead = 0;
			#if DEBUG_XSTREAM
			DBG("decoded %d bytes", ctx->bytesConsumed);
			#endif
			return true;
		}
		ctx->bytesRead++;
	}

	ctx->bytesConsumed = 0;
	return false;
}

void xstream_crlf_init(struct xstream *x, int fd, int init_opts, xstream_read_callback on_read, xstream_write_callback on_write, xstream_error_callback on_error){
	xstream_init(x,fd,on_read, on_write, on_error);
	x->decode_func = xstream_decode_crlf;
	x->decode_opts |= init_opts;
}

void xstream_init(struct xstream *x, int fd, xstream_read_callback on_read, xstream_write_callback on_write, xstream_error_callback on_error){
	memset(x,0,sizeof(*x));
	x->stream_fd.stream.notify_read  = ustream_notify_read;
	x->stream_fd.stream.notify_write = ustream_notify_write;
	x->stream_fd.stream.notify_state = ustream_notify_state;
	ustream_fd_init(&x->stream_fd, fd);
	
	x->on_read_cb = on_read;
	x->on_write_cb = on_write;
	x->on_error_cb = on_error;
	x->decode_func = xstream_decode_default;
}

void xstream_set_decode_func(struct xstream *x, xstream_decode_func decode_func){
	x->decode_func = decode_func;
}

void xstream_free(struct xstream *x){
    if (x->stream_fd.fd.registered){
        ustream_free(&x->stream_fd.stream);
    }
}