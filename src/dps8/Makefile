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
../simhv39-0/scp.c \
../simhv39-0/sim_console.c \
../simhv39-0/sim_ether.c \
../simhv39-0/sim_fio.c \
../simhv39-0/sim_sock.c \
../simhv39-0/sim_tape.c \
../simhv39-0/sim_timer.c \
../simhv39-0/sim_tmxr.c  \
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
../simhv39-0/scp.o \
../simhv39-0/sim_console.o \
../simhv39-0/sim_ether.o \
../simhv39-0/sim_fio.o \
../simhv39-0/sim_sock.o \
../simhv39-0/sim_tape.o \
../simhv39-0/sim_timer.o \
../simhv39-0/sim_tmxr.o \
../decNumber/decContext.o \
../decNumber/decNumber.o 

# for Linux (Ubuntu 12.10 64-bit) or Apple OS/X 10.8
CFLAGS = -I../decNumber -I../simhv39-0 -std=c99 -U__STRICT_ANSI__  -O2 -finline-functions -fgcse-after-reload -fpredictive-commoning -fipa-cp-clone -fno-unsafe-loop-optimizations -fno-strict-overflow -flto -fwhole-program -Wno-unused-result -I . -D_GNU_SOURCE -DUSE_READER_THREAD -DHAVE_DLOPEN=so

# for Linux (Gentoo 4.3.6-r1 p1.0, pie-10.1.5)
# Ubuntu Linux 8.04.4, "Hardy Heron" (which is no longer a supported release) 
#CFLAGS = -I../decNumber -I../simhv39-0 -std=c99 -U__STRICT_ANSI__  -O2 \
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

