# Copyright 2012-2015 by Harry Reed
# Copyright 2016 by Michal Tomek
#
# All rights reserved.
#
# This software is made available under the terms of the
# ICU License -- ICU 1.8.1 and later. 
# See the LICENSE file at the top-level directory of this distribution and
# at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE

# Our Cygwin users are using gcc.
ifeq ($(OS),Windows_NT)
    CC = gcc
    LD = gcc
    SYS := $(shell $(CC) -dumpmachine)
    EXE = .exe
endif

C_SRCS += \
./as8.l \
./as8.y \
./as.c \
./asUtils.c \
./asMisc.c \
./expr.c \
./opcodeTable.c \
./opCodes.c \
./literals.c \
./main.c \
./pseudoOps.c 

OBJS += \
./as.o \
./asUtils.o \
./asMisc.o \
./expr.o \
./opcodeTable.o \
./opCodes.o \
./literals.o \
./main.o \
./pseudoOps.o \
./y.tab.o \
./lex.yy.o

CFLAGS = -std=c99

YACC = bison
LEX = flex

ifeq ($(OS),Windows_NT)
  ifneq (, $(findstring mingw, $(SYS)))
     OBJSG = ../gnulib/asprintf.o ../gnulib/strcasestr.o
     OBJSG2 = ../gnulib/strndup.o
     INDEXfix = -Dindex=strchr
  endif
endif

# rules

all: as8+ as8pp

# Tool invocations
as8+: $(OBJS)
	$(CC) -o "as8+" $(OBJS) $(OBJSG)

opcodeTable.o: y.tab.h

y.tab.o: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c

lex.yy.o: lex.yy.c y.tab.h
	$(CC) $(CFLAGS) -Wno-unused -c lex.yy.c

y.tab.c: as8.y
	$(YACC) -ydv as8.y

y.tab.h: y.tab.c

lex.yy.c: as8.l
	$(LEX) -i as8.l	

as8pp : as8pp.c
	$(CC) as8pp.c $(OBJSG2) -o as8pp $(CFLAGS) $(INDEXfix) -U__STRICT_ANSI__ -O0 -g

# Other Targets
clean:
	-$(RM) $(OBJS) $(C_DEPS) $(EXECUTABLES) y.tab.c y.tab.h lex.yy.c y.output as8+$(EXE) as8pp$(EXE)
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:
