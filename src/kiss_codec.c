#include "kiss_codec.h"

#include "xstream.h"
#include "log.h"
#include "utils.h"

#define KISS_RX_TIMEOUT 1000

enum {
    STATE_IDLE,
    STATE_FRAME_BEGIN,
    STATE_FRAME_HEAD,
    STATE_FRAME_BODY
};

#define FRAME_NORMAL_MODE 0
#define FRAME_ESCAPE_MODE 8

#define KISS_FEND  0xc0
#define KISS_FESC  0xdb
#define KISS_TFEND 0xdc
#define KISS_TFESC 0xdd

#define CHECK_CRC(crc) ((~(crc) & 0xff))

#define DEBUG_CODEC 1

#if DEBUG_CODEC
#define DUMP(s,l) stringdump(s,l)
#else
#define DUMP(s,l)
#endif

static char __buf[512];
static KissFrame _framebuf = {
    .obj = {
        .type = "KISS",
        .data = __buf,
        .dataLen = 0,
        .dataSizeMax = sizeof(__buf)
    }
};

struct uloop_timeout timer; // timer may used for frame decode timeout detection.

static inline void kiss_reset(){
    if (timer.pending){
        uloop_timeout_cancel(&timer);
    }

    _framebuf.obj.dataLen     = 0;
    _framebuf.port        = 0;
    _framebuf.cmd         = 0;
    _framebuf.crc         = 0;
    _framebuf.state       = STATE_IDLE;
    _framebuf.escape      = false;
}

static void kiss_decode_timeout(struct uloop_timeout *t) {
    DBG("kiss rx timeout");
    kiss_reset();
}

static bool kiss_decode_byte(struct xstream_ctx *ctx, uint8_t c, struct KissFrame* frame){
#if DEBUG_CODEC
    if (c == '\r' ) {
        printf("  0x%x - '\\r'\n",c);
    }else if(c == '\n'){
        printf("  0x%x - '\\n'\n",c);
    }else{
        printf("  0x%x - '%c'\n",c,c);
    }
#endif
    
    switch(frame->state){
        case STATE_IDLE:{
            // detects FEND
            if (c == KISS_FEND) {
                kiss_reset();
                timer.cb = kiss_decode_timeout;
                uloop_timeout_set(&timer, KISS_RX_TIMEOUT);
                frame->state = STATE_FRAME_HEAD; // wait for head
                DBG("Frame begin.");
            }
        }
        break;

        case STATE_FRAME_HEAD:{
            frame->cmd = c & 0x0f;
            frame->port = c >> 4 & 0x0f;
            frame->state = STATE_FRAME_BODY;
            DBG("Frame head, cmd: %d, port: %d",frame->cmd, frame->port);
        }
        break;

        case STATE_FRAME_BODY:{
            if (c == KISS_FEND) {
                // Note:
                // We have minor modi on the KISS protocol.
                // Non-Data/Control frame has CRC byte.
                if(frame->cmd != 0x00 && CHECK_CRC(frame->crc) != 0 ) {
                    // crc error, reset states
                    DBG("KISS frame crc check error");
                    kiss_reset();
                }else{
                    DBG("Frame decoded, bytes len: %d", frame->obj.dataLen);
                    DUMP(frame->obj.data, frame->obj.dataLen);
                    frame->state = STATE_IDLE;
                    uloop_timeout_cancel(&timer);
                    return true;
                }
            }else if (c == KISS_FESC) {
                frame->escape = true;
            }else if (c == KISS_TFESC && frame->escape) {
                frame->obj.data[frame->obj.dataLen++] = KISS_FESC;
                frame->escape = false;
            }else if (c == KISS_TFEND && frame->escape) {
                frame->obj.data[frame->obj.dataLen++] = KISS_FEND;
                frame->escape = false;
            }else{
                frame->obj.data[frame->obj.dataLen++] = c;
            }

            // finally check buffer overflow
            if(frame->obj.dataLen > frame->obj.dataSizeMax){
                DBG("KISS frame buffer is full!");
                kiss_reset();
            }
        }
        break;

        default:{
            // ignore all garbage data
        }
        break;
    }

    // calculate/update the data/payload crc
    if(frame->obj.dataLen > 0) { // just calculate
        frame->crc += frame->obj.data[frame->obj.dataLen -1]; // including the crc byte!
    }

    return false;
}

/*
 * decode function called by xstream implementation.
 */
static bool kiss_decode(struct xstream_ctx *ctx, char *data, int *len){
    if (ctx->currentObj == NULL) {
        kiss_reset();
        ctx->currentObj = &_framebuf.obj;
    }

    // cast the generic xobj to kiss_frame
    KissFrame *f = container_of(ctx->currentObj, KissFrame, obj);
    int i = 0;
	for(;i<(*len);i++){
        if (kiss_decode_byte(ctx, data[i],f)) {
            *len = i+1;
            return true;
        }
	}

	return false;
}

static struct xstream_codec _codec_obj_kiss = {
    .name = "kiss",
    .decode_func = kiss_decode,
    .codec_opts = 0
};

const struct xstream_codec* xstream_codec_kiss(){
    return &_codec_obj_kiss;
}