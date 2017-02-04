#CC=gcc
include ../Makefile.mk

#CFLAGS = -std=c99 -I../mingw_include

OBJS = asprintf.o strcasestr.o strndup.o dirent.o mkstemp.o mkstemps.o tempname.o sigaction.o sigprocmask.o

#all : asprintf.o mkstemp sigaction strcasestr.o strndup.o dirent.o
all : gnulib.a

gnulib.a : $(OBJS)
	@echo AR $@
	@$(AR) cr $@ $?

#mkstemp : mkstemp.o mkstemps.o tempname.o
tempname.o : tempname.c tempname.h
#sigaction : sigaction.o sigprocmask.o
strcasestr.o : strcasestr.c str-two-way.h

%.o : %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
 
# Other Targets
clean:
	-$(RM) *.o gnulib.a

#.PHONY: all clean mkstemp sigaction
.PHONY: all clean 
.SECONDARY:
