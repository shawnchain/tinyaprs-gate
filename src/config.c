/*
 * config.c
 *
 *  Created on: 2016年9月7日
 *      Author: shawn
 */


#include "config.h"

#include <stdio.h>
#include "utils.h"
#include "json.h"

static void process_json_value(json_value* value, int depth);
static void process_json_object(json_value* value, int depth);

Config config = {
		.server = "t2xwt.aprs2.net:14580",
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
					.reopen_wait_time = 15,
					.init_wait_time = 3,
					.read_wait_time_ms = 350,
					.keepalive_wait_time = -1,
				},
				{
					.device="/dev/ttyUSB1",
					.model="tinyaprs",
				},
				{
					.device="/dev/ttyUSB2",
					.model="tinyaprs",
				},
				{
					.device="/dev/ttyUSB3",
					.model="tinyaprs",
				},
			}
		,
		.beacon_text="!3012.48N/12008.48Er431.040MHz iGate/TinyAPRS",
		.logfile="/tmp/tinyaprs.log",
};

/**
 * Load config file
 */
int config_init(const char* f){
	//TODO read and parse the json config file
	char buf[8192];
	FILE *file = NULL;
	char c = 0;
	int i = 0;
	json_value *json = NULL;
	if ((file = fopen(f, "r"))) {
		while ((c = getc(file)) != EOF && i < 8191){
			buf[i++] = c;
		}
	}
	buf[i] = 0;
	if(i == 0){
		WARN("*** config_init: open/read %s failed",f);
	}else{
		// parse as json
		char error[512];
		json_settings settings = { 0 };
		json = json_parse_ex(&settings, buf,i-1,error);
		if(json == NULL || json->type != json_object){
			WARN("*** config_init: unable to parse %s, error: %s",f,error);
		}else{
			// convert to config data
			process_json_value(json,0);
		}
	}

	// read into config
	if(json)
		json_value_free(json);
	if(file)
		fclose(file);

	return 0;
}

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
