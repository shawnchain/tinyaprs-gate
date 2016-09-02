/*
 * igate_client.h
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */

#ifndef SRC_TIER2_CLIENT_H_
#define SRC_TIER2_CLIENT_H_

#include <stddef.h>

//typedef struct{
//	const char host[32];
//	unsigned short port;
//	const char user[9];
//	const char pass[9];
//	const char filter[512];
//}tier2_client_config;

/**
 * Initialize the tier2 client connector
 */
int tier2_client_init(const char* host, unsigned short port, const char* user, const char* pass, const char* filter);

/**
 * Run the tier2 client in a single process loop
 */
int tier2_client_run();

/**
 * Send data to tier2 server
 */
int tier2_client_send(const char* data, size_t len);

/**
 * Publish packet to the T2 server
 */
int tier2_client_publish(const char* packet, size_t len);


#endif /* SRC_TIER2_CLIENT_H_ */
