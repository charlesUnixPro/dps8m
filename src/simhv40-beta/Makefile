include ../Makefile.mk

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

CFLAGS += -Wall 

all: simh.a

simh.a : $(OBJS)
	$(AR) r $@ $?

# Other Targets
clean:
	-$(RM) $(OBJS) simh.a tags
	-@echo ' '


.PHONY: all clean dependents
.SECONDARY:

