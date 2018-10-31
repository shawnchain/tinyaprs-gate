/*
 * beacon.h
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#ifndef SRC_BEACON_H_
#define SRC_BEACON_H_

struct tier2_client;
/**
 * Initialize the beacon
 */
int beacon_init(struct tier2_client *c);

#endif /* SRC_BEACON_H_ */
