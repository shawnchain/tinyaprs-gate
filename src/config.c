/*
 * config.c
 *
 *  Created on: 2016年9月7日
 *      Author: shawn
 */


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "json.h"

//static void process_json_value(json_value* value, int depth);
//static void process_json_object(json_value* value, int depth);

static char config_keys[64][32] = {
	"server",
	"callsign",
	"passcode",
	"filter",
	"beacon.symbol",
	"beacon.lat",
	"beacon.lon",
	"beacon.phg",
	"beacon.text",
	"logfile",
	"tnc0.device",
	"tnc0.model",
	"tnc0.name",
	"tnc0.init",
	"tnc0.baudrate",
	"tnc1.device",
	"tnc1.model",
	"tnc1.name",
	"tnc1.init",
	"tnc1.baudrate",
	"tnc2.device",
	"tnc2.model",
	"tnc2.name",
	"tnc2.init",
	"tnc2.baudrate",
	"tnc3.device",
	"tnc3.model",
	"tnc3.name",
	"tnc3.init",
	"tnc3.baudrate",
	""
};

typedef enum{
	server = 0,
	callsign,
	passcode,
	filter,
	beacon_symbol,
	beacon_lat,
	beacon_lon,
	beacon_phg,
	beacon_text,
	logfile,
	tnc0_device,
	tnc0_model,
	tnc0_name,
	tnc0_init,
	tnc0_baudrate,
	tnc1_device,
	tnc1_model,
	tnc1_name,
	tnc1_init,
	tnc1_baudrate,
	tnc2_device,
	tnc2_model,
	tnc2_name,
	tnc2_init,
	tnc2_baudrate,
	tnc3_device,
	tnc3_model,
	tnc3_name,
	tnc3_init,
	tnc3_baudrate,
	not_found = -1,
}config_key;

#if 0
#define isspace(x) (x == ' ' || x == '\t')
#endif

// Note: This function returns a pointer to a substring of the original string.
// If the given string was allocated dynamically, the caller must not overwrite
// that pointer with the returned value, since the original pointer must be
// deallocated using the same allocator with which it was allocated.  The return
// value must NOT be deallocated using free() etc.
char *trimwhitespace(char *str){
  char *end;
  // Trim leading space
  while(isspace(*str)) str++;
  if(*str == 0)  // All spaces?
    return str;
  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;
  // Write new null terminator
  *(end+1) = 0;
  return str;
}

static config_key config_get_key_index(char* key){
	int i = 0;
	for(i = 0;i < 32;i++){
		char* k = config_keys[i];
		if(*k == 0) break;

		if(strcmp(key,k) == 0){
			return i;
		}
	}
	return not_found;
}


#define OVERWRITE_KEY_TOTAL 64
static Config overwriteConfig;
static uint8_t overwriteConfigKeys[OVERWRITE_KEY_TOTAL];
static int8_t overwriteConfigKeyCount = -1; // max 127 keys

static bool config_is_overwritten(char* key){
	config_key keyIndex = config_get_key_index(key);
	int i = 0;
	for(i = 0;i<overwriteConfigKeyCount;i++){
		if(overwriteConfigKeys[i] == keyIndex){
			return true;
		}
	}
	return false;
}

inline static void _assign_value_long(int32_t *out,char* str){
	printf("convert long value: %s\n",str);
	int32_t val = atol(str);
	if(val >=0) *out = val;
}

static int config_get_kv(Config *pconfig, char* key, char* value, size_t bufLen){
	config_key keyIndex = config_get_key_index(key);
	uint32_t *valueLong = (uint32_t*)value;
	switch(keyIndex){

	case server:
		strncpy(value,pconfig->server,bufLen);
		break;
	case callsign:
		strncpy(value,pconfig->callsign,bufLen);
		break;
	case passcode:
		strncpy(value,pconfig->passcode,bufLen);
		break;
	case filter:
		strncpy(value,pconfig->filter,bufLen);
		break;

	case beacon_symbol:
		strncpy(value,pconfig->beacon.symbol,bufLen);
		break;
	case beacon_lat:
		strncpy(value,pconfig->beacon.lat,bufLen);
		break;
	case beacon_lon:
		strncpy(value,pconfig->beacon.lon,bufLen);
		break;
	case beacon_phg:
		strncpy(value,pconfig->beacon.phg,bufLen);
		break;
	case beacon_text:
		strncpy(value,pconfig->beacon.text,bufLen);
		break;

	case logfile:
		strncpy(value,pconfig->logfile,bufLen);
		break;

	case tnc0_device:
		strncpy(value,pconfig->tnc[0].device,bufLen);
		break;
	case tnc0_model:
		strncpy(value,pconfig->tnc[0].model,bufLen);
		break;
	case tnc0_name:
		strncpy(value,pconfig->tnc[0].name,bufLen);
		break;
	case tnc0_init:
		strncpy(value,pconfig->tnc[0].init_cmd,bufLen);
		break;
	case tnc0_baudrate:
		*valueLong = pconfig->tnc[0].baudrate;
		break;

	case tnc1_device:
		strncpy(value,pconfig->tnc[1].device,bufLen);
		break;
	case tnc1_model:
		strncpy(value,pconfig->tnc[1].model,bufLen);
		break;
	case tnc1_name:
		strncpy(value,pconfig->tnc[1].name,bufLen);
		break;
	case tnc1_init:
		strncpy(value,pconfig->tnc[1].init_cmd,bufLen);
		break;
	case tnc1_baudrate:
		*valueLong = pconfig->tnc[1].baudrate;
		break;

	case tnc2_device:
		strncpy(value,pconfig->tnc[2].device,bufLen);
		break;
	case tnc2_model:
		strncpy(value,pconfig->tnc[2].model,bufLen);
		break;
	case tnc2_name:
		strncpy(value,pconfig->tnc[2].name,bufLen);
		break;
	case tnc2_init:
		strncpy(value,pconfig->tnc[2].init_cmd,bufLen);
		break;
	case tnc2_baudrate:
		*valueLong = pconfig->tnc[2].baudrate;
		break;

	case tnc3_device:
		strncpy(value,pconfig->tnc[3].device,bufLen);
		break;
	case tnc3_model:
		strncpy(value,pconfig->tnc[3].model,bufLen);
		break;
	case tnc3_name:
		strncpy(value,pconfig->tnc[3].name,bufLen);
		break;
	case tnc3_init:
		strncpy(value,pconfig->tnc[3].init_cmd,bufLen);
		break;
	case tnc3_baudrate:
		*valueLong = pconfig->tnc[3].baudrate;
		break;

	default:
		INFO("Unknown config key: %s",key);
		break;
	}

	return keyIndex;

}

static int config_set_kv(Config *pconfig, char* key, char* value){
	config_key keyIndex = config_get_key_index(key);
	switch(keyIndex){
	case server:
		strncpy(pconfig->server,value,sizeof(config.server) - 1);
		break;
	case callsign:
		strncpy(pconfig->callsign,value,sizeof(config.callsign) - 1);
		break;
	case passcode:
		strncpy(pconfig->passcode,value,sizeof(config.passcode) - 1);
		break;
	case filter:
		strncpy(pconfig->filter,value,sizeof(config.filter) - 1);
		break;

	case beacon_symbol:
		strncpy(pconfig->beacon.symbol,value,sizeof(config.beacon.symbol) - 1);
		break;
	case beacon_lat:
		strncpy(pconfig->beacon.lat,value,sizeof(config.beacon.lat) - 1);
		break;
	case beacon_lon:
		strncpy(pconfig->beacon.lon,value,sizeof(config.beacon.lon) - 1);
		break;
	case beacon_phg:
		strncpy(pconfig->beacon.phg,value,sizeof(config.beacon.phg) - 1);
		break;
	case beacon_text:
		strncpy(pconfig->beacon.text,value,sizeof(config.beacon.text) - 1);
		break;

	case logfile:
		strncpy(pconfig->logfile,value,sizeof(config.logfile) - 1);
		break;

	case tnc0_device:
		strncpy(pconfig->tnc[0].device,value,sizeof(config.tnc[0].device) - 1);
		break;
	case tnc0_model:
		strncpy(pconfig->tnc[0].model,value,sizeof(config.tnc[0].model) - 1);
		break;
	case tnc0_name:
		strncpy(pconfig->tnc[0].name,value,sizeof(config.tnc[0].name) - 1);
		break;
	case tnc0_init:
		strncpy(pconfig->tnc[0].init_cmd,value,sizeof(config.tnc[0].init_cmd) - 1);
		break;
	case tnc0_baudrate:
		_assign_value_long(&(pconfig->tnc[0].baudrate), value);
		break;

	case tnc1_device:
		strncpy(pconfig->tnc[1].device,value,sizeof(config.tnc[1].device) - 1);
		break;
	case tnc1_model:
		strncpy(pconfig->tnc[1].model,value,sizeof(config.tnc[1].model) - 1);
		break;
	case tnc1_name:
		strncpy(pconfig->tnc[1].name,value,sizeof(config.tnc[1].name) - 1);
		break;
	case tnc1_init:
		strncpy(pconfig->tnc[1].init_cmd,value,sizeof(config.tnc[1].init_cmd) - 1);
		break;
	case tnc1_baudrate:
		_assign_value_long(&(pconfig->tnc[1].baudrate), value);
		break;

	case tnc2_device:
		strncpy(pconfig->tnc[2].device,value,sizeof(config.tnc[2].device) - 1);
		break;
	case tnc2_model:
		strncpy(pconfig->tnc[2].model,value,sizeof(config.tnc[2].model) - 1);
		break;
	case tnc2_name:
		strncpy(pconfig->tnc[2].name,value,sizeof(config.tnc[2].name) - 1);
		break;
	case tnc2_init:
		strncpy(pconfig->tnc[2].init_cmd,value,sizeof(config.tnc[2].init_cmd) - 1);
		break;
	case tnc2_baudrate:
		_assign_value_long(&(pconfig->tnc[2].baudrate), value);
		break;

	case tnc3_device:
		strncpy(pconfig->tnc[3].device,value,sizeof(config.tnc[3].device) - 1);
		break;
	case tnc3_model:
		strncpy(pconfig->tnc[3].model,value,sizeof(config.tnc[3].model) - 1);
		break;
	case tnc3_name:
		strncpy(pconfig->tnc[3].name,value,sizeof(config.tnc[3].name) - 1);
		break;
	case tnc3_init:
		strncpy(pconfig->tnc[3].init_cmd,value,sizeof(config.tnc[3].init_cmd) - 1);
		break;
	case tnc3_baudrate:
		_assign_value_long(&(pconfig->tnc[3].baudrate), value);
		break;

	default:
		INFO("Unknown config: %s, %s",key,value);
		break;
	}

	return keyIndex;
}

static bool read_ini_file(FILE *file){
	char aline[512];
	char ov[512];
	while(fgets(aline,sizeof(aline) - 1,file) != 0){
		// skip the empty or comment line
		if(aline[0] == 0 || aline[0] == '#') continue;

		// parse the foo=bar or foo='bar'
		char *s = aline;
		char *k = s, *v = 0;
		int i=0;
		while(s[i] != 0){
			// split by '='
			if(s[i] == '='){
				s[i] = 0;
				v = s + i + 1;
				break;
			}
			i++;
		}
		if(!k || !v) continue;

		k = trimwhitespace(k);
		v = trimwhitespace(v);

		if(config_is_overwritten(k)){
			config_key keyIndex = config_get_key_index(k);
			if(keyIndex == tnc0_baudrate){
				// QUICK & DIRTY workaround for number values
				config.tnc[0].baudrate = overwriteConfig.tnc[0].baudrate;
			}else{
				memset(ov,0,sizeof(ov));
				config_get_kv(&overwriteConfig,k,ov,sizeof(ov) -1);
				config_set_kv(&config,k,ov);
				INFO("%s is overwritten from %s to %s",k,v,ov);
			}
		}else{
			DBG("setting %s: %s",k,v);
			config_set_kv(&config,k,v);
		}
	}
	return true;
}

#if 0
static void read_json_file(FILE *file){
	char buf[8192];
	char c = 0;
	int i = 0;
	json_value *json = NULL;
	while ((c = getc(file)) != EOF && i < 8191){
		buf[i++] = c;
	}

	buf[i] = 0;
	if(i > 0){
		// parse as json
		char error[512];
		json_settings settings = { 0 };
		json = json_parse_ex(&settings, buf,i-1,error);
		if(json == NULL || json->type != json_object){
			WARN("*** config_init: unable to parse %s, error: %s",file,error);
		}else{
			// convert to config data
			process_json_value(json,0);
		}
	}

	// read into config
	if(json)
		json_value_free(json);
}
#endif

Config config = {
		.server = "asia.aprs2.net:14580",
		.callsign="N0CALL",
		.passcode="-1",
		.filter="r/30.2731/120.1543/50",
		.tnc ={
				{
					#ifdef __linux__
					.device="/dev/ttyUSB0",
					#else
					.device="/dev/tty.SLAB_USBtoUART",
					#endif
					.model="tinyaprs",
					.baudrate=9600,
					.reopen_wait_time = 15,
					.current_reopen_wait_time = 15,
					.init_wait_time = 3,
					.read_wait_time_ms = 350,
					.keepalive_wait_time = -1,
				},
				{
					.device="/dev/ttyUSB1",
					.model="tinyaprs",
					.baudrate=9600,
				},
				{
					.device="/dev/ttyUSB2",
					.model="tinyaprs",
					.baudrate=9600,
				},
				{
					.device="/dev/ttyUSB3",
					.model="tinyaprs",
					.baudrate=9600,
				},
			}
		,
		.beacon = {
			.symbol="R&",
			.lat = "3012.48N",
			.lon = "12008.48E",
			.text = "431.040MHz iGate/TinyAPRS"
		},
		.logfile="/var/log/tinyaprs.log",
};

/**
 * Load config file
 */
int config_init(const char* f){
	//TODO read and parse the json config file
	FILE *file = NULL;
	file = fopen(f, "rt");
	if(!file){
		WARN("*** config_init: read config file failed, %s. Using command line parameters or defaults",f);
		return 0;
	}else{
		INFO("Reading config file: %s",f);
	}

	//read_json_file(file);
	read_ini_file(file);

	if(file)
		fclose(file);

	return 0;
}

int config_overwrite_kv(char* key, char* value){
	int i = 0;
	if(overwriteConfigKeyCount < 0){
		// perform init;
		memset(&overwriteConfig,0,sizeof(Config));
		for(i = 0;i<OVERWRITE_KEY_TOTAL;i++){
			overwriteConfigKeys[i] = 0;
		}
		overwriteConfigKeyCount = 0;
	}
	overwriteConfigKeys[overwriteConfigKeyCount++] = config_set_kv(&overwriteConfig,key,value);
	return 0;
}



#if 0
static void print_depth_shift(int depth) {
	int j;
	for (j = 0; j < depth; j++) {
		printf(" ");
	}
}

static void process_json_object(json_value* value, int depth) {
	int length, x;
	if (value == NULL) {
		return;
	}
	length = value->u.object.length;
	for (x = 0; x < length; x++) {
		print_depth_shift(depth);
		printf("object[%d].name = %s\n", x, value->u.object.values[x].name);
		process_json_value(value->u.object.values[x].value, depth + 1);
	}
}

static void process_json_array(json_value* value, int depth) {
	int length, x;
	if (value == NULL) {
		return;
	}
	length = value->u.array.length;
	printf("array\n");
	for (x = 0; x < length; x++) {
		process_json_value(value->u.array.values[x], depth);
	}
}

static void process_json_value(json_value* value, int depth) {
	if (value == NULL) {
		return;
	}
	if (value->type != json_object) {
		print_depth_shift(depth);
	}
	switch (value->type) {
	case json_none:
		printf("none\n");
		break;
	case json_object:
		process_json_object(value, depth + 1);
		break;
	case json_array:
		process_json_array(value, depth + 1);
		break;
	case json_integer:
		printf("int: %10" PRId64 "\n", value->u.integer);
		break;
	case json_double:
		printf("double: %f\n", value->u.dbl);
		break;
	case json_string:
		printf("string: %s\n", value->u.string.ptr);
		break;
	case json_boolean:
		printf("bool: %d\n", value->u.boolean);
		break;
	default:
		break;
	}
}
#endif
