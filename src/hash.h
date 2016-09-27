/*
 * hash.h
 *
 *  Created on: 2016年9月27日
 *      Author: shawn
 */

#ifndef SRC_HASH_H_
#define SRC_HASH_H_

#include <stdio.h>
#include <stdint.h>

typedef uint16_t UINT2;
typedef uint32_t UINT4;

typedef struct {
  uint32_t state[4];		/* state (ABCD) */
  uint32_t count[2];		/* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];	/* input buffer */
} MD5_CTX;

#ifdef __cplusplus
extern "C" {
#endif

/** md5 for string
 *  parameters:
 *           string: the string to md5
 *           digest: store the md5 digest
 *  return: 0 for success, != 0 fail
*/
int hash_md5_string(char *string, unsigned char digest[16]);

/** md5 for file
 *  parameters:
 *           filename: the filename whose content to md5
 *           digest: store the md5 digest
 *  return: 0 for success, != 0 fail
*/
int hash_md5_file(char *filename, unsigned char digest[16]);

/** md5 for buffer
 *  parameters:
 *           buffer: the buffer to md5
 *           len: the buffer length
 *           digest: store the md5 digest
 *  return: 0 for success, != 0 fail
*/
int hash_md5_buffer(char *buffer, unsigned int len, unsigned char digest[16]);

void hash_md5_init (MD5_CTX *);

void hash_md5_update (MD5_CTX *, unsigned char *, unsigned int);

void hash_md5_final (unsigned char [16], MD5_CTX *);

#ifdef __cplusplus
}
#endif

#endif /* SRC_HASH_H_ */
