/*
 * igate_client.h
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */

#ifndef SRC_IS_CONNECTOR_H_
#define SRC_IS_CONNECTOR_H_

#include <stddef.h>

/**
 * Initialize the tier2 client connector
 */
int is_connector_init(const char* server);

/**
 * Run the tier2 client in a single process loop
 */
int is_connector_run();

/**
 * Send data to tier2 server
 */
int is_connector_send(const char* data, size_t len);

/**
 * Publish packet to the T2 server
 */
int is_connector_publish(const char* packet, size_t len);


#endif /* SRC_IS_CONNECTOR_H_ */
