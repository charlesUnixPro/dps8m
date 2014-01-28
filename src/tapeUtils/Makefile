CFLAGS = -g -O0 -Wall -std=c99 -U__STRICT_ANSI__
LDFLAGS = -g
ETF_OBJS = extract_tape_files.o bit36.o simhtapes.o
MF_OBJS = mfile.o bit36.o simhtapes.o
DT_OBJS = dumpTape.o bit36.o
RST_OBJS = restore.o bit36.o 
SCN_OBJS = scan.o bit36.o
DIS_OBJS = disasm.o bit36.o
PK_OBJS = pack.o bit36.o
UP_OBJS = unpack.o bit36.o
EBL0_OBJS = extract_bound_bootload_0.o bit36.o
MXA_OBJS = mxarc.o bit36.o
PBM_OBJS = print_bind_map.o bit36.o
P9_OBJS = pack9.o

EXES = extract_tape_files mfile dumpTape restore scan disasm pack extract_bound_bootload_0 unpack mxarc print_bind_map pack9

OBJS = $(ETF_OBJS) $(MF_OBJS) $(DT_OBJS) $(RST_OBJS) $(SCN_OBJS) $(DIS_OBJS) $(PK_OBJS) $(EBL0_OBJS) $(UP_OBS) $(MXA_OBJS) $(PBM_OBJS) $(P9_OBJS)

all : $(EXES)

extract_tape_files : $(ETF_OBJS)
mfile : $(MF_OBJS)
dumpTape : $(DT_OBJS)
restore : $(RST_OBJS)
scan : $(SCN_OBJS)
disasm : $(DIS_OBJS)
pack : $(PK_OBJS)
extract_bound_bootload_0 : $(EBL0_OBJS)
unpack : $(UP_OBJS)
mxarc : $(MXA_OBJS)
print_bind_map : $(PBM_OBJS)
pack9 : $(P9_OBJS)

extract_tape_files.o : extract_tape_files.c bit36.h simhtapes.h mst.h
bit36.o : bit36.c bit36.h
mfile.o : mfile.c bit36.h simhtapes.h mst.h
simhtapes.o : simhtapes.c simhtapes.h
dumpTape.o : dumpTape.c bit36.h
restore.o : restore.c bit36.h mst.h
scan.o : scan.c bit36.h mst.h
disasm.o : disasm.c bit36.h
pack.o : pack.c bit36.h
extract_bound_bootload_0.o : extract_bound_bootload_0.c mst.h
unpack.o : unpack.c bit36.h
mxarc.o : mxarc.c bit36.h mst.h
print_bind_map.o : print_bind_map.c bit36.h
pack9.o  : pack9.c bit36.h

clean:
	-rm $(OBJS)  $(EXES)

