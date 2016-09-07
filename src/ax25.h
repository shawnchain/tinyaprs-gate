/*
 * ax25.h
 *
 *  Created on: 2016年9月3日
 *      Author: shawn
 */

#ifndef SRC_AX25_H_
#define SRC_AX25_H_

#include "utils.h"

#define CONFIG_AX25_FRAME_BUF_LEN 330 	//AX25 frame buffer lenght.
#define CONFIG_AX25_RPT_LST 1			//Enable repeaters listing in AX25 frames.


#define AX25_MIN_FRAME_LEN 18

/**
 * CRC computation on correct AX25 packets should
 * give this result (don't ask why).
 */
#define AX25_CRC_CORRECT 0xF0B8

struct AX25Msg; // fwd declaration

/**
 * Type for AX25 messages callback.
 */
typedef void (*ax25_callback_t)(struct AX25Msg *msg);

#if CONFIG_AX25_STAT
typedef struct AX25Stat{
	uint32_t rx_ok;
	uint32_t tx_ok;
	uint32_t rx_err;
}AX25Stat;
#endif

/**
 * AX25 Protocol context.
 */
typedef struct AX25Ctx
{
	uint8_t buf[CONFIG_AX25_FRAME_BUF_LEN]; ///< buffer for received chars
	int *ch;        ///< KFile used to access the physical medium
	size_t frm_len;   ///< received frame length.
	uint16_t crc_in;  ///< CRC for current received frame
	uint16_t crc_out; ///< CRC of current sent frame
	ax25_callback_t hook; ///< Hook function to be called when a message is received
	bool pass_through;
	bool sync;   ///< True if we have received a HDLC flag.
	bool escape; ///< True when we have to escape the following char.
	uint8_t dcd_state;
	bool dcd;

#if CONFIG_AX25_STAT
	volatile AX25Stat stat;
#endif
} AX25Ctx;


/**
 * AX25 Call sign.
 */
typedef struct AX25Call
{
	char call[6]; ///< Call string, max 6 character
	uint8_t ssid; ///< SSID (secondary station ID) for the call
} AX25Call;

/**
 * Create an AX25Call structure on the fly.
 * \param str callsign, can be 6 characters or shorter.
 * \param id  ssid associated with the callsign.
 */
#define AX25_CALL(str, id) {.call = (str), .ssid = (id) }

/**
 * Maximum number of Repeaters in a AX25 message.
 */
#define AX25_MAX_RPT 8


/**
 * AX25 Message.
 * Used to handle AX25 sent/received messages.
 */
typedef struct AX25Msg
{
	AX25Call src;  ///< Source adress
	AX25Call dst;  ///< Destination address
	#if CONFIG_AX25_RPT_LST
	AX25Call rpt_lst[AX25_MAX_RPT]; ///< List of repeaters
	uint8_t rpt_cnt; ///< Number of repeaters in this message
	uint8_t rpt_flags; ///< Has-been-repeated flags for each repeater (bit-mapped)
	#define AX25_REPEATED(msg, idx) ((msg)->rpt_flags & BV(idx))
	#endif
	uint16_t ctrl; ///< AX25 control field
	uint8_t pid;   ///< AX25 PID field
	const uint8_t *info; ///< Pointer to the info field (payload) of the message
	size_t len;    ///< Payload length
} AX25Msg;

#if CONFIG_AX25_RPT_LST
	#define AX25_SET_REPEATED(msg, idx, val) \
		do \
		{ \
			if (val) \
				(msg)->rpt_flags |= BV(idx) ; \
			else \
				(msg)->rpt_flags &= ~BV(idx) ; \
		} while(0)
#endif

#define AX25_CTRL_UI      0x03
#define AX25_PID_NOLAYER3 0xF0

/**
 * \name HDLC flags.
 * These should be moved in
 * a separated HDLC related file one day...
 * \{
 */
#define HDLC_FLAG  0x7E
#define HDLC_RESET 0x7F
#define AX25_ESC   0x1B
/* \} */


/**
 * Declare an AX25 path.
 * \param dst the destination callsign for the path, \see AX25_CALL
 *        for a handy way to create a callsign on the fly.
 * \param src the source callsign for the path, \see AX25_CALL
 *        for a handy way to create a callsign on the fly.
 *
 * Additional optional callsigns can be specified at the end of this macro
 * in order to add repeater callsigns or specific unproto paths.
 *
 * This macro can be used to simply path array declaration.
 * Should be used in this way:
 * \code
 * AX25Call path[] = AX25_PATH(AX25_CALL("abcdef", 0), AX25_CALL("ghjklm", 0), AX25_CALL("wide1", 1), AX25_CALL("wide2", 2));
 * \endcode
 *
 * The declared path can then be passed to ax25_sendVia().
 */
#define AX25_PATH(dst, src, ...) { dst, src, ## __VA_ARGS__ }

void ax25_poll(AX25Ctx *ctx);
void ax25_sendVia(AX25Ctx *ctx, const AX25Call *path, size_t path_len, const void *_buf, size_t len);
void ax25_sendRaw(AX25Ctx *ctx, const void *_buf, size_t len);
void ax25_putchar(AX25Ctx *ctx, uint8_t c);

void ax25_sendMsg(AX25Ctx *ctx, const AX25Msg *msg);
/**
 * Send an AX25 frame on the channel.
 * \param ctx AX25 context to operate on.
 * \param dst the destination callsign for the frame, \see AX25_CALL
 *        for a handy way to create a callsign on the fly.
 * \param src the source callsign for the frame, \see AX25_CALL
 *        for a handy way to create a callsign on the fly.
 * \param buf payload buffer.
 * \param len length of the payload.
 *
 * \see ax25_sendVia() if you want to send a frame with a specific path.
 */
#define ax25_send(ctx, dst, src, buf, len) ax25_sendVia(ctx, ({static AX25Call __path[]={dst, src}; __path;}), 2, buf, len)
void ax25_init(AX25Ctx *ctx, int *channel, ax25_callback_t hook);


int ax25_print(char *buf, size_t len, const AX25Msg *msg);

int ax25_decode(uint8_t *data, size_t len, AX25Msg *msg);

#endif /* SRC_AX25_H_ */
