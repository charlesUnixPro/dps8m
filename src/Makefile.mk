# Copyright 2014-2016 by Charles Anthony
# Copyright 2016 by Michal Tomek
#
# All rights reserved.
#
# This software is made available under the terms of the
# ICU License -- ICU 1.8.1 and later. 
# See the LICENSE file at the top-level directory of this distribution and
# at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE

ifeq ($(CROSS),MINGW64)
  CC = x86_64-w64-mingw32-gcc
  LD = x86_64-w64-mingw32-gcc
  AR = x86_64-w64-mingw32-ar
  EXE = .exe
else
#CC = gcc
#LD = gcc
CC = clang
LD = clang
endif

# for Linux (Ubuntu 12.10 64-bit) or Apple OS/X 10.8
#CFLAGS  = -g -O0
CFLAGS  = -g -O3

# Our Cygwin users are using gcc.
ifeq ($(OS),Windows_NT)
    CC = gcc
    LD = gcc
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),FreeBSD)
      CFLAGS += -I /usr/local/include
      LDFLAGS += -L/usr/local/lib
    endif
endif

CFLAGS += -std=c99
CFLAGS += -U__STRICT_ANSI__  
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DUSE_READER_THREAD
CFLAGS += -DUSE_INT64

ifneq ($(CROSS),MINGW64)
CFLAGS += -DHAVE_DLOPEN=so
endif

# Clang generates warning messages for code it generates itself...
CFLAGS += -Wno-array-bounds

LDFLAGS += -g

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

