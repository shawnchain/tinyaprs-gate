#
# Copyright (c) 2015 Justin Lopyright (c) 2016 Shawn Chain
# Author: Shawn Chain (shawn.chain@gmail.com)
# https://github.com/shawnchain/tinyaprs-gate/
#

ifeq ($(PREFIX),)
PREFIX := $(shell [ -d /opt/local ] && echo /opt/local || echo /usr )
endif

CC ?= gcc
CFLAGS += -Wall -ggdb
HEADERS = tinyaprs_gate.h tier2_client.h tnc_connector.h beacon.h utils.h

tinyaprsgate: tinyaprs_gate.o tier2_client.o tnc_connector.o beacon.o utils.o
	$(CC) $(LDFLAGS) -o $@ $^ 

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

all: tinyaprsgate

install: tinyaprsgate 
	cp -f tinyaprsgate $(PREFIX)/sbin/

clean:
	rm -f tinyaprsgate *.o
