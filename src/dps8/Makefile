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

CFLAGS += -Wunused-argument \
-Wunused-function \
-Wunused-label \
-Wunused-parameter \
-Wunused-value \
-Wunused-variable \
-Wunused \
-Wextra

CFLAGS += -Wunreachable-code

#CFLAGS += -Wsign-conversion
zzzCFLAGS += \
-Wabi \
-Waddress-of-temporary \
-Waddress \
-Waggregate-return \
-Wall \
-Wambiguous-member-template \
-Warc-abi \
-Warc-non-pod-memaccess \
-Warc-retain-cycles \
-Warc-unsafe-retained-assign \
-Warc \
-Watomic-properties \
-Wattributes \
-Wavailability \
-Wbad-function-cast \
-Wbind-to-temporary-copy \
-Wbitwise-op-parentheses \
-Wbool-conversions \
-Wbuiltin-macro-redefined \
-Wc++-compat \
-Wc++0x-compat \
-Wc++0x-extensions \
-Wcast-align \
-Wcast-qual \
-Wchar-align \
-Wchar-subscripts \
-Wcomment \
-Wcomments \
-Wconditional-uninitialized \
-Wconversion \
-Wctor-dtor-privacy \
-Wcustom-atomic-properties \
-Wdeclaration-after-statement \
-Wdelegating-ctor-cycles \
-Wdelete-non-virtual-dtor \
-Wdeprecated-declarations \
-Wdeprecated-implementations \
-Wdeprecated-writable-strings \
-Wdeprecated \
-Wdisabled-optimization \
-Wdiscard-qual \
-Wdiv-by-zero \
-Wduplicate-method-arg \
-Weffc++ \
-Wempty-body \
-Wendif-labels \
-Wexit-time-destructors \
-Wextra-tokens \
-Wextra \
-Wformat-extra-args \
-Wformat-nonliteral \
-Wformat-zero-length \
-Wformat \
-Wformat=2 \
-Wfour-char-constants \
-Wglobal-constructors \
-Wgnu-designator \
-Wgnu \
-Wheader-hygiene \
-Widiomatic-parentheses \
-Wignored-qualifiers \
-Wimplicit-atomic-properties \
-Wimplicit-function-declaration \
-Wimplicit-int \
-Wimplicit \
-Wimport \
-Wincompatible-pointer-types \
-Winit-self \
-Winitializer-overrides \
-Winline \
-Wint-to-pointer-cast \
-Winvalid-offsetof \
-Winvalid-pch \
-Wlarge-by-value-copy \
-Wliteral-range \
-Wlocal-type-template-args \
-Wlogical-op-parentheses \
-Wlong-long \
-Wmain \
-Wmicrosoft \
-Wmismatched-tags \
-Wmissing-braces \
-Wmissing-declarations \
-Wmissing-field-initializers \
-Wmissing-format-attribute \
-Wmissing-include-dirs \
-Wmissing-noreturn \
-Wmost \
-Wmultichar \
-Wnested-externs \
-Wnewline-eof \
-Wnon-gcc \
-Wnon-virtual-dtor \
-Wnonnull \
-Wnonportable-cfstrings \
-Wnull-dereference \
-Wobjc-nonunified-exceptions \
-Wold-style-cast \
-Wold-style-definition \
-Wout-of-line-declaration \
-Woverflow \
-Woverlength-strings \
-Woverloaded-virtual \
-Wpacked \
-Wparentheses \
-Wpointer-arith \
-Wpointer-to-int-cast \
-Wprotocol \
-Wreadonly-setter-attrs \
-Wredundant-decls \
-Wreorder \
-Wreturn-type \
-Wself-assign \
-Wsemicolon-before-method-body \
-Wsequence-point \
-Wshadow \
-Wshorten-64-to-32 \
-Wsign-compare \
-Wsign-promo \
-Wsizeof-array-argument \
-Wstack-protector \
-Wstrict-aliasing \
-Wstrict-overflow \
-Wstrict-prototypes \
-Wstrict-selector-match \
-Wsuper-class-method-mismatch \
-Wswitch-default \
-Wswitch-enum \
-Wswitch \
-Wsynth \
-Wtautological-compare \
-Wtrigraphs \
-Wtype-limits \
-Wundeclared-selector \
-Wuninitialized \
-Wunknown-pragmas \
-Wunnamed-type-template-args \
-Wunneeded-internal-declaration \
-Wunneeded-member-function \
-Wunused-argument \
-Wunused-exception-parameter \
-Wunused-function \
-Wunused-label \
-Wunused-member-function \
-Wunused-parameter \
-Wunused-value \
-Wunused-variable \
-Wunused \
-Wused-but-marked-unused \
-Wvariadic-macros \
-Wvector-conversions \
-Wvla \
-Wvolatile-register-var \
-Wwrite-strings
 

 

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

H_SRCS  = dps8_addrmods.h  dps8_disk.h       dps8_iom.h          dps8_utils.h
H_SRCS += dps8_append.h    dps8_eis.h        dps8_loader.h       utarray.h
H_SRCS += dps8_bar.h       dps8_faults.h     dps8_math.h         uthash.h
H_SRCS += dps8_clk.h       dps8.h            dps8_mt.h           utlist.h
H_SRCS += dps8_console.h   dps8_hw_consts.h  dps8_opcodetable.h
H_SRCS += dps8_cpu.h       dps8_iefp.h       dps8_scu.h
H_SRCS += dps8_decimal.h   dps8_ins.h        dps8_sys.h


all: tags locallibs_then_dps8 

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
	-$(RM) $(OBJS) $(C_DEPS) $(EXECUTABLES) dps8 tags $(C_SRCS:.c=.d)

cleaner: clean
	(cd ../simh; $(MAKE) clean)
	(cd ../decNumber; $(MAKE) clean)

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(C_SRCS:.c=.d)
