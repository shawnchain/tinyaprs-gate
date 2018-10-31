#
# Copyright (c) 2015 Justin Lopyright (c) 2016 Shawn Chain
# Author: Shawn Chain (shawn.chain@gmail.com)
# https://github.com/shawnchain/tinyaprs-gate/
#

GIT_VERSION := $(shell git describe --abbrev=6 --always --tags)

CC ?= gcc

CFLAGS += -Wall -ggdb -DDEBUG -DVERSION=\"$(GIT_VERSION)\"

HEADERS = uloop.h usock.h ustream.h ulog.h utils.h

LIB_OBJ = uloop.o \
	uloop.o \
	usock.o \
	ustream.o \
	ustream-fd.o \
	utils.o

libubox.a: $(LIB_OBJ)
	ar rcs libubox.a $(LIB_OBJ)

libs: libubox.a

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.a *.o
