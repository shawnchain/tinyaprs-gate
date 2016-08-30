#
# Copyright (c) 2015 Justin Liu
# Author: Justin Liu <rssnsj@gmail.com>
# https://github.com/rssnsj/minivtun
#

ifeq ($(PREFIX),)
PREFIX := $(shell [ -d /opt/local ] && echo /opt/local || echo /usr )
endif

CC ?= gcc
CFLAGS += -Wall -ggdb
HEADERS = tinyaprs_gate.h tier2_client.h tnc_connector.h utils.h

tinyaprsgate: tinyaprs_gate.o tier2_client.o tnc_connector.o utils.o
	$(CC) $(LDFLAGS) -o $@ $^ 

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

all: tinyaprsgate

install: tinyaprsgate 
	cp -f tinyaprsgate $(PREFIX)/sbin/

clean:
	rm -f tinyaprsgate *.o
