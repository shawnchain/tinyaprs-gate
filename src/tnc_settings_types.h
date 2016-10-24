/*
 * tnc_settings_types.h
 *
 *  Created on: 2016年10月24日
 *      Author: shawn
 */

#ifndef SRC_TNC_SETTINGS_TYPES_H_
#define SRC_TNC_SETTINGS_TYPES_H_

#pragma pack(1)
typedef struct AX25Call{
	char call[6];
	uint8_t ssid;
}AX25Call;

#pragma pack(1)
typedef struct CallData{
	AX25Call destCall;
	AX25Call myCall;
	AX25Call path1;
	AX25Call path2;
	//AX25Call path3[7];
}CallData;

/*
 * The settings types
 */
typedef enum{
	SETTINGS_CALL = 1,
	SETTINGS_TEXT,
	SETTINGS_PARAMS,
	SETTINGS_COMMIT = 15
}SettingsType;

/*
 * The settings parameter keys
 */
typedef enum {
	SETTINGS_SYMBOL,
	SETTINGS_RUN_MODE,
	SETTINGS_BEACON_INTERVAL,
}SettingsParamKey;

#pragma pack(1)
typedef struct BeaconParams{
	uint8_t		symbol[2];		// Symbol table and the index
	uint16_t	interval; 		// Beacon send interval
	uint8_t		type;			// 0 = smart, 1 = fixed interval
}BeaconParams; // 5 bytes

#pragma pack(1)
typedef struct RfParams{
	uint8_t txdelay;
	uint8_t txtail;
	uint8_t persistence;
	uint8_t slot_time;
	uint8_t duplex;
}RfParams; // 5 bytes

#pragma pack(1)
typedef struct{
	uint8_t run_mode;		// the run mode ,could be 0|1|2
	BeaconParams beacon;	// the beacon parameters
	RfParams rf;			// the rf parameters
} SettingsData; // 11 bytes


#endif /* SRC_TNC_SETTINGS_TYPES_H_ */
