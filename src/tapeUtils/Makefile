CFLAGS = -g -O0 -Wall -std=c99 -U__STRICT_ANSI__
LDFLAGS = -g
ETF_OBJS = extract_tape_files.o bit36.o simhtapes.o
MF_OBJS = mfile.o bit36.o simhtapes.o

EXES = extract_tape_files mfile
OBJS = $(ETF_OBJS) $(MF_OBJS)

all : $(EXES)

extract_tape_files : $(ETF_OBJS)
mfile : $(MF_OBJS)

extract_tape_files.o : extract_tape_files.c bit36.h simhtapes.h mst.h
bit36.o : bit36.c bit36.h
mfile.o : mfile.c bit36.h simhtapes.h mst.h
simhtapes.o : simhtapes.c simhtapes.h
clean:
	-rm $(OBJS)  $(EXES)

