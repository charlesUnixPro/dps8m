# Copyright 2012-2013 by Harry Reed
#
# All rights reserved.
#
# This software is made available under the terms of the
# ICU License -- ICU 1.8.1 and later. 
# See the LICENSE file at the top-level directory of this distribution and
# at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE

C_SRCS += \
./ExprEval.c \
./as.c \
./asUtils.c \
./asMisc.c \
../../dps8/dps8_opcodetable.c \
./main.c \
./pseudoOps.c 

OBJS += \
./ExprEval.o \
./as.o \
./asUtils.o \
./asMisc.o \
../../dps8/dps8_opcodetable.o \
./main.o \
./pseudoOps.o
 
CFLAGS = -I../../dps8 -I../../simhv39-0 -std=c99

all: as8

# Tool invocations
as8: $(OBJS) 
	gcc  -o "as8" $(OBJS) 
	
# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) as8
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

