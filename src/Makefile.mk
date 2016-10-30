# Copyright 2014-2016 by Charles Anthony
#
# All rights reserved.
#
# This software is made available under the terms of the
# ICU License -- ICU 1.8.1 and later. 
# See the LICENSE file at the top-level directory of this distribution and
# at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE

#CC = gcc
#LD = gcc
CC = clang
LD = clang

# Our Cygwin users are using gcc.
ifeq ($(OS),Windows_NT)
    CC = gcc
    LD = gcc
endif

# for Linux (Ubuntu 12.10 64-bit) or Apple OS/X 10.8
#CFLAGS  = -g -O0
CFLAGS  = -g -O3

#CFLAGS = -m32
#CFLAGS = -m64

CFLAGS += -I../decNumber -I../simhv40-beta -I ../include 

CFLAGS += -std=c99 -U__STRICT_ANSI__  
#CFLAGS += -std=c99 -U__STRICT_ANSI__  -Wconversion

# CFLAGS += -finline-functions -fgcse-after-reload -fpredictive-commoning -fipa-cp-clone -fno-unsafe-loop-optimizations -fno-strict-overflow -Wno-unused-result 

CFLAGS += -D_GNU_SOURCE -DUSE_READER_THREAD -DHAVE_DLOPEN=so 
CFLAGS += -DUSE_INT64
#CFLAGS += -DMULTIPASS
# Clang generates warning messages for code it generates itself...
CFLAGS += -Wno-array-bounds
LDFLAGS += -g
#CFLAGS += -pg
#LDFLAGS += -pg

MAKEFLAGS += --no-print-directory

%.o : %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

# This file is included as '../Makefile.mk', so it's local include needs the ../
ifneq (,$(wildcard ../Makefile.local))
$(warning ####)
$(warning #### Using ../Makefile.local)
$(warning ####)
include ../Makefile.local
endif

