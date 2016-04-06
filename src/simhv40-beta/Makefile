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

#CFLAGS += -Wall 
CFLAGS += -Wno-array-bounds -Wno-unevaluated-expression


all: simh.a

simh.a : $(OBJS)
	@echo AR $@
	@$(AR) cr $@ $?

# Other Targets
clean:
	@echo CLEAN simh
	-@$(RM) $(OBJS) simh.a tags


.PHONY: all clean dependents
.SECONDARY:

