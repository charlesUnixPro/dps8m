CFLAGS = -g -O0 -Wall -std=c99 -U__STRICT_ANSI__
LDFLAGS = -g
ETF_OBJS = extract_tape_files.o bit36.o simhtapes.o
MF_OBJS = mfile.o bit36.o simhtapes.o
DT_OBJS = dumpTape.o bit36.o
DF_OBJS = dumpFile.o bit36.o
RST_OBJS = restore.o bit36.o 
SCN_OBJS = scan.o bit36.o
DIS_OBJS = disasm.o bit36.o
DIS2_OBJS = dis.o bit36.o
PK_OBJS = pack.o bit36.o
UP_OBJS = unpack.o bit36.o
EBL0_OBJS = extract_bound_bootload_0.o bit36.o
MXA_OBJS = mxarc.o bit36.o
PBM_OBJS = print_bind_map.o bit36.o
P9_OBJS = pack9.o
RTOC_OBJS = restore_toc.o bit36.o
RTT_OBJS = restore_tape.o bit36.o simhtapes.o
P72_OBJS = p72_to_acsii.o bit36.o
P72B_OBJS = p72_to_binary.o bit36.o
P72A_OBJS = p72archive_to_acsii.o bit36.o
P72S_OBJS = p72archive_to_segment.o bit36.o
DD_OBJS = diskDump.o bit36.o
ABA_OBJS = p72archive_bind_to_ascii.o bit36.o

EXES = extract_tape_files mfile dumpTape dumpFile restore scan disasm dis pack extract_bound_bootload_0 unpack mxarc print_bind_map pack9 restore_toc restore_tape p72_to_acsii p72archive_to_acsii p72archive_to_segment diskDump p72archive_bind_to_ascii dumpDisk dumpDiskRecords purge dumpCard viipunch pack8 cat36 reload

OBJS = $(ETF_OBJS) $(MF_OBJS) $(DT_OBJS) $(DF_OBJS) $(RST_OBJS) $(SCN_OBJS) $(DIS_OBJS) $(DIS2_OBJS) $(PK_OBJS) $(EBL0_OBJS) $(UP_OBS) $(MXA_OBJS) $(PBM_OBJS) $(P9_OBJS) $(RTOC_OBJS) $(RTT_OBJS) $(ABA_OBS)

all : $(EXES)

reload: reload.o
extract_tape_files : $(ETF_OBJS)
mfile : $(MF_OBJS)
dumpTape : $(DT_OBJS)
dumpFile : $(DF_OBJS)
restore : $(RST_OBJS)
scan : $(SCN_OBJS)
disasm : $(DIS_OBJS)
dis : $(DIS2_OBJS)
pack : $(PK_OBJS)
extract_bound_bootload_0 : $(EBL0_OBJS)
unpack : $(UP_OBJS)
mxarc : $(MXA_OBJS)
print_bind_map : $(PBM_OBJS)
pack9 : $(P9_OBJS)
pack8 : pack8.o
restore_toc: $(RTOC_OBJS)
restore_tape: $(RTT_OBJS)
p72_to_acsii: $(P72_OBJS)
p72_to_binary: $(P72B_OBJS)
p72archive_to_acsii: $(P72A_OBJS)
p72archive_to_segment: $(P72S_OBJS)
diskDump: $(DD_OBJS)
p72archive_bind_to_ascii: $(ABA_OBJS)
dumpDisk: dumpDisk.o bit36.o
dumpDiskRecords: dumpDiskRecords.o bit36.o
cat36: cat36.o bit36.o

extract_tape_files.o : extract_tape_files.c bit36.h simhtapes.h mst.h
bit36.o : bit36.c bit36.h
mfile.o : mfile.c bit36.h simhtapes.h mst.h
simhtapes.o : simhtapes.c simhtapes.h
dumpTape.o : dumpTape.c bit36.h
dumpFile.o : dumpFile.c bit36.h
restore.o : restore.c bit36.h mst.h
scan.o : scan.c bit36.h mst.h
disasm.o : disasm.c bit36.h
pack.o : pack.c bit36.h
extract_bound_bootload_0.o : extract_bound_bootload_0.c mst.h
unpack.o : unpack.c bit36.h
mxarc.o : mxarc.c bit36.h mst.h
print_bind_map.o : print_bind_map.c bit36.h
pack9.o : pack9.c bit36.h
restore_toc.o : restore_toc.c bit36.h
restore_tape.o : restore_tape.c bit36.h mst.h simhtapes.h
purge.o : purge.c
clean:
	-rm $(OBJS)  $(EXES)

