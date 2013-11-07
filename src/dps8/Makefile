C_SRCS += \
./dps8_addrmods.c \
./dps8/dps8_append.c \
./dps8_bar.c \
./dps8_cpu.c \
./dps8_eis.c \
./dps8_ins.c \
./dps8_iom.c \
./dps8_loader.c \
./dps8_lp.c \
./dps8_math.c \
./dps8_mt.c \
./dps8_opcodetable.c \
./dps8_stddev.c \
./dps8_sys.c \
./dps8_utils.c \
./dps8_decimal.c \
./dps8_faults.c \
../simh/scp.c \
../simh/sim_console.c \
../simh/sim_disk.c \
../simh/sim_ether.c \
../simh/sim_fio.c \
../simh/sim_serial.c \
../simh/sim_sock.c \
../simh/sim_tape.c \
../simh/sim_timer.c \
../simh/sim_tmxr.c  \
../decNumber/decContext.c \
../decNumber/decNumber.c 


OBJS += \
./dps8_addrmods.o \
./dps8_append.o \
./dps8_bar.o \
./dps8_cpu.o \
./dps8_eis.o \
./dps8_ins.o \
./dps8_iom.o \
./dps8_loader.o \
./dps8_lp.o \
./dps8_math.o \
./dps8_mt.o \
./dps8_opcodetable.o \
./dps8_stddev.o \
./dps8_sys.o \
./dps8_utils.o \
./dps8_decimal.o \
./dps8_faults.o \
../simh/scp.o \
../simh/sim_console.o \
../simh/sim_disk.o \
../simh/sim_ether.o \
../simh/sim_fio.o \
../simh/sim_serial.o \
../simh/sim_sock.o \
../simh/sim_tape.o \
../simh/sim_timer.o \
../simh/sim_tmxr.o \
../decNumber/decContext.o \
../decNumber/decNumber.o 

# for Linux (Ubuntu 12.10 64-bit) or Apple OS/X 10.8
CFLAGS = -g -I../decNumber -I../simh -std=c99 -U__STRICT_ANSI__  -O2 -finline-functions -fgcse-after-reload -fpredictive-commoning -fipa-cp-clone -fno-unsafe-loop-optimizations -fno-strict-overflow -flto -fwhole-program -Wno-unused-result -I . -D_GNU_SOURCE -DUSE_READER_THREAD -DHAVE_DLOPEN=so


# for Linux (Gentoo 4.3.6-r1 p1.0, pie-10.1.5)
# Ubuntu Linux 8.04.4, "Hardy Heron" (which is no longer a supported release) 
#CFLAGS = -I../decNumber -I../simh -std=c99 -U__STRICT_ANSI__  -O2 \
#  -fgcse-after-reload \
#  -fno-unsafe-loop-optimizations -fno-strict-overflow  \
#  -I . -D_GNU_SOURCE -DUSE_READER_THREAD -DHAVE_DLOPEN=so

# For OS/X
#LIBS = -lm -lpthread -ldl   -flto -fwhole-program

LIBS = -lm -lrt -lpthread -ldl   -flto -fwhole-program

all: dps8

# Tool invocations
dps8: $(OBJS) 
	gcc  -o "dps8" $(OBJS) $(CFLAGS) $(LIBS)
	
# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) dps8
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

