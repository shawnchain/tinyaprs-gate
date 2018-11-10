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
CFLAGS += -Wall -ggdb -DDEBUG -DVERSION=\"$(GIT_VERSION)\" -I. -Ilibubox

CXX ?= g++
CPPFLAGS += -Wall -ggdb -DDEBUG -DVERSION=\"$(GIT_VERSION)\" -I. -Ilibubox

HEADERS = \
	tier2_client.h \
	beacon.h \
	utils.h \
	serial.h \
	slre.h \
	kiss_codec.h \
	ax25.h \
	json.h \
	config.h \
	log.h \
	modem.h 

all: tinyaprsd tinyaprs-util tinyaprs-cfg 

OBJ = tinyaprsd.o \
	tier2_client.o \
	beacon.o \
	utils.o \
	serial.o \
	slre.o \
	kiss_codec.o \
	ax25.o \
	json.o \
	config.o \
	log.o \
	modem.o

LIB_OBJ = libubox/uloop.o \
	libubox/uloop.o \
	libubox/usock.o \
	libubox/ustream.o \
	libubox/ustream-fd.o \
	libubox/utils.o

LIB_OBJ += libubox/xstream.o

libubox.a: $(LIB_OBJ)
	ar rcs libubox.a $(LIB_OBJ)

libs: libubox.a

tinyaprsd: $(OBJ) $(LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ 

tinyaprs-util: aprs_util.o utils.o log.o hash.o
	$(CC) $(LDFLAGS) -o $@ $^
	
tinyaprs-cfg: tnc_config.o utils.o log.o serial.o
	$(CC) $(LDFLAGS) -o $@ $^

tncmock: tnc_mock.o utils.o log.o serial.o ax25.o kiss_codec.o $(LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

libubox/%.o: libubox/%.c $(HEADERS)
	$(CC) $(CFLAGS)  -c -o $@ $<

libubox_cpp/%.o: libubox_cpp/%.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) -c -o $@ $<

install: tinyaprsd 
	cp -f tinyaprsd $(PREFIX)/sbin/
	cp -f tinyaprs-util $(PREFIX)/sbin/
	cp -f tinyaprs-cfg  $(PREFIX)/sbin/

clean:
	rm -f tinyaprsd tinyaprs-util  tinyaprs-cfg tncmock *.o libubox/*.o libubox_cpp/*.o *.a
