CFLAGS = -g -O0 -Wall -std=c99 -U__STRICT_ANSI__
LDFLAGS = -g
ETF_OBJS = extract_tape_files.o bit36.o

EXES = extract_tape_files
OBJS = $(ETF_OBJS)

all : $(EXES)

extract_tape_files : $(ETF_OBJS)

extract_tape_files.o : extract_tape_files.c bit36.h

bit36.o : bit36.c bit36.h

clean:
	-rm $(OBJS)  $(EXES)

