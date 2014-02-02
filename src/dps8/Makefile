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

C_SRCS  = ./dps8_addrmods.c 
C_SRCS += ./dps8_append.c
C_SRCS += ./dps8_bar.c
C_SRCS += ./dps8_console.c
C_SRCS += ./dps8_clk.c
C_SRCS += ./dps8_cpu.c
C_SRCS += ./dps8_disk.c
C_SRCS += ./dps8_eis.c
C_SRCS += ./dps8_ins.c
C_SRCS += ./dps8_iom.c
C_SRCS += ./dps8_loader.c
C_SRCS += ./dps8_lp.c
C_SRCS += ./dps8_math.c
C_SRCS += ./dps8_mt.c
C_SRCS += ./dps8_opcodetable.c
C_SRCS += ./dps8_scu.c
C_SRCS += ./dps8_stddev.c
C_SRCS += ./dps8_sys.c
C_SRCS += ./dps8_utils.c
C_SRCS += ./dps8_decimal.c
C_SRCS += ./dps8_faults.c
C_SRCS += ./dps8_iefp.c

OBJS  := $(patsubst %.c,%.o,$(C_SRCS))

H_SRCS = dps8.h  utarray.h  uthash.h  utlist.h dps8_console.h dps8_disk.h dps8_iom.h dps8_mt.h dps8_utils.h 

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

#dps8.h :  ../simh/sim_defs.h ../simh/sim_tape.h

dps8_console.o : dps8_iom.h
dps8_cpu.o : dps8_utils.h
dps8_decimal.o : ../decNumber/decNumber.h ../decNumber/decNumberLocal.h
dps8_disk.o : dps8_iom.h dps8_disk.h 
dps8_iom.o : dps8_utils.h dps8_iom.h dps8_mt.h dps8_disk.h
dps8_loader.o : utlist.h
dps8_mt.o : dps8_iom.h dps8_mt.h
dps8_scu.o : dps8_utils.h 
dps8_sys.c : dps8_iom.h dps8_mt.h dps8_disk.h dps8_utils.h
dps8_utils.o :  ../simh/sim_defs.h dps8_utils.h

$(OBJS) : dps8.h ../simh/sim_defs.h ../simh/sim_tape.h
