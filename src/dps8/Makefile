C_SRCS = \
./dps8_addrmods.c \
./dps8_append.c \
./dps8_bar.c \
./dps8_console.c \
./dps8_clk.c \
./dps8_cpu.c \
./dps8_disk.c \
./dps8_eis.c \
./dps8_ins.c \
./dps8_iom.c \
./dps8_loader.c \
./dps8_lp.c \
./dps8_math.c \
./dps8_mt.c \
./dps8_opcodetable.c \
./dps8_scu.c \
./dps8_stddev.c \
./dps8_sys.c \
./dps8_utils.c \
./dps8_decimal.c \
./dps8_faults.c 


OBJS = \
./dps8_addrmods.o \
./dps8_append.o \
./dps8_bar.o \
./dps8_console.o \
./dps8_clk.o \
./dps8_cpu.o \
./dps8_disk.o \
./dps8_eis.o \
./dps8_ins.o \
./dps8_iom.o \
./dps8_loader.o \
./dps8_lp.o \
./dps8_math.o \
./dps8_mt.o \
./dps8_opcodetable.o \
./dps8_scu.o \
./dps8_stddev.o \
./dps8_sys.o \
./dps8_utils.o \
./dps8_decimal.o \
./dps8_faults.o 

H_SRCS = dps8.h  utarray.h  uthash.h  utlist.h

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
#LDFLAGS += -Wl,--cref -Wl,-Map=dps8.map

# Dese make debugging hard; also, I don't belive that -fwhole-program is appropriate
# CFLAGS += -flto -fwhole-program

# Temporary to tidy -Wall
CFLAGS += -DQUIET_UNUSED

# for Linux (Gentoo 4.3.6-r1 p1.0, pie-10.1.5)
# Ubuntu Linux 8.04.4, "Hardy Heron" (which is no longer a supported release) 
#CFLAGS = -I../decNumber -I../simh -std=c99 -U__STRICT_ANSI__  -O2 \
#  -fgcse-after-reload \
#  -fno-unsafe-loop-optimizations -fno-strict-overflow  \
#  -I . -D_GNU_SOURCE -DUSE_READER_THREAD -DHAVE_DLOPEN=so

# For OS/X
#LIBS = -lm -lpthread -ldl   -flto -fwhole-program

LOCALLIBS = ../simh/simh.a ../decNumber/decNumber.a

LIBS = $(LOCALLIBS)  -lm -lrt -lpthread -ldl -lefence 

all: locallibs_then_dps8 tags

dps8 : $(OBJS) $(LOCALLIBS) 
	$(LD)  -o "dps8" $(OBJS) $(LDFLAGS) $(LIBS)
	
locallibs_then_dps8 :
	(cd ../simh; $(MAKE))
	(cd ../decNumber; $(MAKE))
	$(MAKE) dps8

# Other Targets

tags : $(C_SRCS) $(H_SRCS)
	ctags $(C_SRCS) $(H_SRCS) ../simh/*.[ch] ../decNumber/*.[ch]

.PHONY : clean cleaner

clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) dps8 tags

cleaner: clean
	(cd ../simh; $(MAKE) clean)
	(cd ../decNumber; $(MAKE) clean)

dps8.h :  ../simh/sim_defs.h ../simh/sim_tape.h
dps8_utils.o :  ../simh/sim_defs.h
dps8_decimal.o : ../decNumber/decNumber.h ../decNumber/decNumberLocal.h
dps8_loader.o : utlist.h
$(OBJS) : dps8.h

