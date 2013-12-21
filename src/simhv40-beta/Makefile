C_SRCS = \
scp.c \
sim_console.c \
sim_disk.c \
sim_ether.c \
sim_fio.c \
sim_serial.c \
sim_sock.c \
sim_tape.c \
sim_timer.c \
sim_tmxr.c


OBJS = \
scp.o \
sim_console.o \
sim_disk.o \
sim_ether.o \
sim_fio.o \
sim_serial.o \
sim_sock.o \
sim_tape.o \
sim_timer.o \
sim_tmxr.o

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

all: simh.a

simh.a : $(OBJS)
	$(AR) r $@ $?

# Other Targets
clean:
	-$(RM) $(OBJS) simh.a
	-@echo ' '


.PHONY: all clean dependents
.SECONDARY:

