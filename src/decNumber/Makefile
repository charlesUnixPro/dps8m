C_SRCS += \
decContext.c \
decNumber.c 


OBJS += \
decContext.o \
decNumber.o 

# for Linux (Ubuntu 12.10 64-bit) or Apple OS/X 10.8
CFLAGS = -g -I../decNumber -I../simh -std=c99 -U__STRICT_ANSI__  -O0 -finline-functions -fgcse-after-reload -fpredictive-commoning -fipa-cp-clone -fno-unsafe-loop-optimizations -fno-strict-overflow -Wno-unused-result -I . -D_GNU_SOURCE -DUSE_READER_THREAD -DHAVE_DLOPEN=so -Wall -DUSE_INT64

# Dese make debugging hard; also, I don't belive that -fwhole-program is appropriate
# CFLAGS += -flto -fwhole-program

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

