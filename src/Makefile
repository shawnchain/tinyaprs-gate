#
# Copyright (c) 2015 Justin Lopyright (c) 2016 Shawn Chain
# Author: Shawn Chain (shawn.chain@gmail.com)
# https://github.com/shawnchain/tinyaprs-gate/
#

ifeq ($(PREFIX),)
PREFIX := $(shell [ -d /opt/local ] && echo /opt/local || echo /usr )
endif

CC ?= gcc
CFLAGS += -Wall -ggdb -DDEBUG
HEADERS = tinyaprs_gate.h tier2_client.h tnc_connector.h beacon.h utils.h serial_port.h slre.h kiss.h ax25.h json.h config.h

tinyaprs: tinyaprs_gate.o tier2_client.o tnc_connector.o beacon.o utils.o serial_port.o slre.o kiss.o ax25.o json.o config.o
	$(CC) $(LDFLAGS) -o $@ $^ 

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

all: tinyaprs

install: tinyaprs 
	cp -f tinyaprs $(PREFIX)/sbin/

clean:
	rm -f tinyaprs *.o
