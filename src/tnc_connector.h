/*
 * tnc_connector.h
 *
 *  Created on: 2016年8月30日
 *      Author: shawn
 */

#ifndef SRC_TNC_CONNECTOR_H_
#define SRC_TNC_CONNECTOR_H_

/**
 * Initialize the TNC
 */
int tnc_init(const char* devName, unsigned int baudrate, const char* model, char** initCmds);

/**
 *
 */
int tnc_run();

#endif /* SRC_TNC_CONNECTOR_H_ */
