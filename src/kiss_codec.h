/*
 * kiss_codec.h
 *
 *  Created on: 2016年9月2日
 *      Author: shawn
 */

#include<stdbool.h>
#include "xstream.h"

typedef struct KissFrame{
    struct xstream_obj    obj;     // the base object 

    unsigned char   port;
    unsigned char   cmd;
    unsigned char   crc;           // crc byte

    // internal state
    unsigned int    state;
    bool            escape;
}KissFrame;

/*
 * Get the KISS codec object
 */
const struct xstream_codec* xstream_codec_kiss();