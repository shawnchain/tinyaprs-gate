#
# Copyright (c) 2015 Justin Lopyright (c) 2016 Shawn Chain
# Author: Shawn Chain (shawn.chain@gmail.com)
# https://github.com/shawnchain/tinyaprs-gate/
#

ifeq ($(PREFIX),)
PREFIX := $(shell [ -d /opt/local ] && echo /opt/local || echo /usr )
endif

GIT_VERSION := $(shell git describe --abbrev=6 --always --tags)

CC ?= gcc
CFLAGS += -Wall -ggdb -DDEBUG -DVERSION=\"$(GIT_VERSION)\"
HEADERS = tinyaprs_gate.h is_connector.h tnc_connector.h beacon.h utils.h serial_port.h slre.h kiss.h ax25.h json.h config.h log.h iokit.h

all: tinyaprs aprsutil tncfg

tinyaprs: tinyaprs_gate.o is_connector.o tnc_connector.o beacon.o utils.o serial_port.o slre.o kiss.o ax25.o json.o config.o log.o iokit.o
	$(CC) $(LDFLAGS) -o $@ $^ 

aprsutil: aprs_util.o utils.o log.o hash.o
	$(CC) $(LDFLAGS) -o $@ $^
	
tncfg: tnc_config.o utils.o log.o serial_port.o
	$(CC) $(LDFLAGS) -o $@ $^

tncmock: tnc_mock.o utils.o log.o serial_port.o ax25.o kiss2.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

install: tinyaprs 
	cp -f tinyaprs $(PREFIX)/sbin/
	cp -f aprsutil $(PREFIX)/sbin/
	cp -f tncfg  $(PREFIX)/sbin/

clean:
	rm -f tinyaprs aprsutil aprsutil tncfg tncmock *.o
