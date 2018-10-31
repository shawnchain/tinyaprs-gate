/*
 * igate_client.h
 *
 *  Created on: 2016年8月29日
 *      Author: shawn
 */

#ifndef TIER2_CLIENT_H_
#define TIER2_CLIENT_H_

#include "xstream.h"
#include "utils.h"

struct tier2_client{
    struct uloop_timeout timer;
    struct xstream stream;

    char server[64];
    unsigned int state;
    int sockfd;
    time_t last_reconnect;
    time_t last_recv;
    time_t last_keepalive;
    struct sockaddr_inx server_addr;
};

/**
 * Initialize the tier2 client connector
 */
int tier2_client_init(struct tier2_client *c, const char* server);

/**
 * Send data to tier2 server
 */
int tier2_client_send(struct tier2_client *c, const char* data, size_t len);

/**
 * Publish packet to the T2 server
 */
int tier2_client_publish(struct tier2_client *c, const char* packet, size_t len);

/**
 * Check if client is connected and logged in
 */
bool tier2_client_is_verified(struct tier2_client *c);

#endif /* TIER2_CLIENT_H_ */
