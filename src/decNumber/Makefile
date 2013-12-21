C_SRCS = \
decContext.c \
decNumber.c 


OBJS = \
decContext.o \
decNumber.o 

#CC = gcc
#LD = gcc
CC = clang
LD = clang

# for Linux (Ubuntu 12.10 64-bit) or Apple OS/X 10.8
CFLAGS  = -g -O0
#CFLAGS  = -g -O3
CFLAGS += -I../decNumber -I../simh 
CFLAGS += -std=c99 -U__STRICT_ANSI__  
# CFLAGS += -finline-functions -fgcse-after-reload -fpredictive-commoning -fipa-cp-clone -fno-unsafe-loop-optimizations -fno-strict-overflow -Wno-unused-result 
# CFLAGS += -I . 
CFLAGS += -D_GNU_SOURCE -DUSE_READER_THREAD -DHAVE_DLOPEN=so 
CFLAGS += -Wall 
CFLAGS += -DUSE_INT64

LDFLAGS = -g

all: decNumber.a

decNumber.a : $(OBJS)
	$(AR) r $@ $?

# Other Targets
clean:
	-$(RM) $(OBJS) decNumber.a
	-@echo ' '

$(OBJS) : decContext.h decimal128.h decNumber.h decQuad.h decDouble.h decimal32.h decNumberLocal.h decSingle.h decDPD.h decimal64.h  decPacked.h


.PHONY: all clean dependents
.SECONDARY:

