/*
 * ax25.c
 *
 *  Created on: 2016年9月3日
 *      Author: shawn
 */

#include <string.h>

#include "ax25.h"
#include "utils.h"

/*
 * Decode the CALL field, assume addr is a fix-size array
 */
#define DECODE_CALL(buf, addr) \
	for (unsigned i = 0; i < sizeof((addr)); i++) \
	{ \
		char c = (*(buf)++ >> 1); \
		(addr)[i] = (c == ' ') ? '\x0' : c; \
	}

#ifndef BV
	/** Convert a bit value to a binary flag. */
	#define BV(x)  (1<<(x))
#endif


//TODO - construct the ax25 message
int ax25_make(const char* src, const char* dst, const char** rpt, const char* payload){
	return -1;
}

/*
 * The decoded ax25 frame contains:
 * | DST_ID(7) |SRC_ID(7) | RPT_LIST(7 * 8) | CTRL(0x03) | PID(0xF0) | PAYLOAD | LEN |
 */
int ax25_decode(uint8_t *data, size_t len, AX25Msg *msg){
	bzero(msg,sizeof(AX25Msg));

	uint8_t *buf = data;

	DECODE_CALL(buf, msg->dst.call);
	msg->dst.ssid = (*buf++ >> 1) & 0x0F;

	DECODE_CALL(buf, msg->src.call);
	msg->src.ssid = (*buf >> 1) & 0x0F;

	INFO("SRC[%.6s-%d], DST[%.6s-%d]\n", msg->src.call, msg->src.ssid, msg->dst.call, msg->dst.ssid);

	/* Repeater addresses */
	#if CONFIG_AX25_RPT_LST
		for (msg->rpt_cnt = 0; !(*buf++ & 0x01) && (msg->rpt_cnt < countof(msg->rpt_lst)); msg->rpt_cnt++)
		{
			DECODE_CALL(buf, msg->rpt_lst[msg->rpt_cnt].call);
			msg->rpt_lst[msg->rpt_cnt].ssid = (*buf >> 1) & 0x0F;
			AX25_SET_REPEATED(msg, msg->rpt_cnt, (*buf & 0x80));

			INFO("RPT%d[%.6s-%d]%c\n", msg->rpt_cnt,
				msg->rpt_lst[msg->rpt_cnt].call,
				msg->rpt_lst[msg->rpt_cnt].ssid,
				(AX25_REPEATED(msg, msg->rpt_cnt) ? '*' : ' '));
		}
	#else
		while (!(*buf++ & 0x01))
		{
			char rpt[6];
			uint8_t ssid;
			DECODE_CALL(buf, rpt);
			ssid = (*buf >> 1) & 0x0F;
			LOG_INFO("RPT[%.6s-%d]\n", rpt, ssid);
		}
	#endif

	msg->ctrl = *buf++;
	if (msg->ctrl != AX25_CTRL_UI)
	{
		WARN("Only UI frames are handled, got [%02X]\n", msg->ctrl);
		return -1;
	}

	msg->pid = *buf++;
	if (msg->pid != AX25_PID_NOLAYER3)
	{
		WARN("Only frames without layer3 protocol are handled, got [%02X]\n", msg->pid);
		return -1;
	}

	msg->len = len /*- 2*/ - (buf - data);
	msg->info = buf;
	INFO("DATA: %.*s\n", msg->len, msg->info);
	return 1;
}


static int print_call(char* buf, const AX25Call *call){
	int i = 0;
	i = sprintf(buf, "%.6s", call->call);
	if (call->ssid)
		i+= sprintf(buf + i, "-%d", call->ssid);
	return i;
}


/**
 * Print a AX25 message in TNC-2 packet monitor format.
 * \param ch a kfile channel where the message will be printed.
 * \param msg the message to be printed.
 */
int ax25_print(char *buf, size_t len, const AX25Msg *msg){
	char src[10],dst[10],path[128];
	print_call(src,&msg->src);
	print_call(dst,&msg->dst);

	char* p = path;
	for (int i = 0; i < msg->rpt_cnt; i++){
		*p++ = ',';
		p += print_call(p, &msg->rpt_lst[i]);
		/* Print a '*' if packet has already been transmitted
		 * by this repeater */
		if (AX25_REPEATED(msg, i))
			*p++ = '*';
	}
	*p = 0;
	return snprintf(buf,len,"%s>%s%s:%.*s\n\r",src,dst,path,(int)msg->len,msg->info);
}


///**
// * Init the AX25 protocol decoder.
// *
// * \param ctx AX25 context to init.
// * \param channel Used to gain access to the physical medium
// * \param hook Callback function called when a message is received
// */
//void ax25_init(AX25Ctx *ctx, KFile *channel, ax25_callback_t hook)
//{
//	ASSERT(ctx);
//	ASSERT(channel);
//
//	memset(ctx, 0, sizeof(*ctx));
//	ctx->ch = channel;
//	ctx->hook = hook;
//	// decode the AX.25 frame needs extra memory but necessary acting as digipeater
//	// or for displaying/debug purpose.
//	// AS a TNC modem with KISS protocol used, pass_though could be enabled(set=1)
//	ctx->pass_through = 0;
//	ctx->crc_in = ctx->crc_out = CRC_CCITT_INIT_VAL;
//}


///**
// * Check if there are any AX25 messages to be processed.
// * This function read available characters from the medium and search for
// * any AX25 messages.
// * If a message is found it is decoded and the linked callback executed.
// * This function may be blocking if there are no available chars and the KFile
// * used in \a ctx to access the medium is configured in blocking mode.
// *
// * \param ctx AX25 context to operate on.
// */
//void ax25_poll(AX25Ctx *ctx)
//{
//	int c;
//
//	while ((c = kfile_getc(ctx->ch)) != EOF)
//	{
//		if (!ctx->escape && c == HDLC_FLAG)
//		{
//			if (ctx->frm_len >= AX25_MIN_FRAME_LEN)
//			{
//				if (ctx->crc_in == AX25_CRC_CORRECT)
//				{
//					LOG_INFO("Frame found!\n");
//#if CONFIG_AX25_STAT
//					ATOMIC(ctx->stat.rx_ok++);
//#endif
//					if (ctx->pass_through) {
//						if (ctx->hook) {
//							//TODO: make MSG union and pass to hook
//							ctx->hook(NULL);
//						}
//					} else {
//						ax25_decode(ctx);
//					}
//				}
//				else
//				{
//					LOG_INFO("CRC error, computed [%04X]\n", ctx->crc_in);
//#if CONFIG_AX25_STAT
//					ATOMIC(ctx->stat.rx_err++);
//#endif
//				}
//			}
//			ctx->sync = true;
//			ctx->crc_in = CRC_CCITT_INIT_VAL;
//			ctx->frm_len = 0;
//
//			ctx->dcd_state = 0;
//			ctx->dcd = false;
//			continue;
//		}
//
//		if (!ctx->escape && c == HDLC_RESET)
//		{
//			LOG_INFO("HDLC reset\n");
//			ctx->sync = false;
//			ctx->dcd = false;
//			continue;
//		}
//
//		if (!ctx->escape && c == AX25_ESC)
//		{
//			ctx->escape = true;
//			continue;
//		}
//
//		if (ctx->sync)
//		{
//			if (ctx->frm_len < CONFIG_AX25_FRAME_BUF_LEN)
//			{
//				ctx->buf[ctx->frm_len++] = c;
//				ctx->crc_in = updcrc_ccitt(c, ctx->crc_in);
//
//				if (ctx->dcd_state == 1 && c == AX25_PID_NOLAYER3) {
//					ctx->dcd_state ++;
//					ctx->dcd = true;
//				}
//
//				if (ctx->dcd_state == 0 && c == AX25_CTRL_UI) {
//					ctx->dcd_state ++;
//				}
//			}
//			else
//			{
//				LOG_INFO("Buffer overrun");
//				ctx->sync = false;
//				ctx->dcd = false;
//#if CONFIG_AX25_STAT
//				ATOMIC(ctx->stat.rx_err++);
//#endif
//			}
//		}
//		ctx->escape = false;
//	}
//
//	if (kfile_error(ctx->ch))
//	{
//		LOG_ERR("Channel error [%04x]\n", kfile_error(ctx->ch));
//		kfile_clearerr(ctx->ch);
//#if CONFIG_AX25_STAT
//		if(ctx->dcd){
//			ATOMIC(ctx->stat.rx_err++);
//		}
//#endif
//		ctx->dcd = false;
//	}
//}

