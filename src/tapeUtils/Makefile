CFLAGS = -g -O0 -Wall -std=c99 -U__STRICT_ANSI__
LDFLAGS = -g
ETF_OBJS = extract_tape_files.o bit36.o simhtapes.o
MF_OBJS = mfile.o bit36.o simhtapes.o
DT_OBJS = dumpTape.o bit36.o
RST_OBJS = restore.o bit36.o 

EXES = extract_tape_files mfile dumpTape restore

OBJS = $(ETF_OBJS) $(MF_OBJS) $(DT_OBJS) $(RST_OBJS)

all : $(EXES)

extract_tape_files : $(ETF_OBJS)
mfile : $(MF_OBJS)
dumpTape : $(DT_OBJS)
restore : $(RST_OBJS)

extract_tape_files.o : extract_tape_files.c bit36.h simhtapes.h mst.h
bit36.o : bit36.c bit36.h
mfile.o : mfile.c bit36.h simhtapes.h mst.h
simhtapes.o : simhtapes.c simhtapes.h
dumpTape.o : dumpTape.c bit36.h
restore.o : restore.c bit36.h mst.h

clean:
	-rm $(OBJS)  $(EXES)

