#!/bin/bash

T=./source/
S=./tmp/
TAPES=./

echo "Tape restore"

../tapeUtils/restore_tape $S $TAPES/12.6EXEC.tap
../tapeUtils/restore_tape $S $TAPES/12.6LDD_STANDARD.tap
../tapeUtils/restore_tape $S $TAPES/12.6UNBUNDLED.tap
../tapeUtils/restore_tape $S $TAPES/12.6MISC.tap

echo "Bulk extract"

# *.alm, *.pl1, etc
(cd $S && find . -type f \( -name "*.absin" -o \
                            -name "*.alm" -o \
                            -name "*.bcpl" -o \
                            -name "*.bind" -o \
                            -name "*.bindmap" -o \
                            -name "*.bind_fnp" -o \
                            -name "*.cds" -o \
                            -name "*.checker*" -o \
                            -name "*.cmf" -o \
                            -name "*.compin" -o \
                            -name "*.compout" -o \
                            -name "*.control" -o \
                            -name "*.ct" -o \
                            -name "*.dcl" -o \
                            -name "*.ec" -o \
                            -name "*.fortran" -o \
                            -name "*.ge" -o \
                            -name "*.header" -o \
                            -name "*.info" -o \
                            -name "*.info.1" -o \
                            -name "*.iodt" -o \
                            -name "*.ld" -o \
                            -name "*.lisp" -o \
                            -name "*.list*" -o \
                            -name "*.map355" -o \
                            -name "*.message" -o \
                            -name "*.mexp" -o \
                            -name "*.pascal" -o \
                            -name "*.pl1" -o \
                            -name "*.pldt" -o \
                            -name "*.pnotice" -o \
                            -name "*.qedx" -o \
                            -name "*.rtmf" -o \
                            -name "*.search" -o \
                            -name "*.src" -o \
                            -name "*.ssl" -o \
                            -name "*.table" -o \
                            -name "*.teco" -o \
                            -name "*.ti" -o \
                            -name "*.ttf" -o \
                            -name "tut_" -o \
                            -name "*.xdw" \
 \)  -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72_to_acsii $S/'{}' $T/'{}'

echo "Specials extract"

(cd $S && for N in \
                   documentation/facilities_data_dir/edoc_db \
                   documentation/MR12.3/error_msgs.toc.compout \
                   documentation/MR12.3/error_msgs.compout/0 \
                   documentation/MR12.3/error_msgs.compout/1 \
                   documentation/MR12.3/SRB \
                   documentation/MR12.3/SIB \
                   documentation/MR12.3/TRs_fixed_in_MR12.3 \
                   documentation/MR12.5/error_msgs.compout/0 \
                   documentation/MR12.5/error_msgs.compout/1 \
                   documentation/MR12.5/system_book_ \
                   documentation/MR12.6/error_msgs.compout/0 \
                   documentation/MR12.6/error_msgs.compout/1 \
                   documentation/MR12.6/MR12.6_SRB+SIB \
                   documentation/MR12.6/MR12.6_SRB+SIB.retrieve.map \
                   documentation/MR12.6/system_book_ \
                   library_dir_dir/crossref/total.crossref/0 \
                   library_dir_dir/crossref/total.crossref/1 \
                   library_dir_dir/crossref/total.crossref/2 \
                   library_dir_dir/crossref/total.crossref/3 \
                   library_dir_dir/crossref/inst_dir/total.crossref/0 \
                   library_dir_dir/crossref/inst_dir/total.crossref/1 \
                   library_dir_dir/crossref/inst_dir/total.crossref/2 \
                   library_dir_dir/crossref/inst_dir/total.crossref/3 \
                   library_dir_dir/mcs/info/macros_asm \
                   library_dir_dir/system_library_1/info/hardcore_checker_map \
                   library_dir_dir/system_library_tools/object/psp_info_ \
                   ; do
                       echo $N
                       ../../tapeUtils/p72_to_acsii $N ../$T/$N
                     done)

echo "Archives extract"

# *.s.archive
(cd $S && find . -type f -name "*.s.archive" -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72archive_to_acsii $S/'{}' $T/'{}'
(cd $S && find . -type f -name "*.incl.archive" -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72archive_to_acsii $S/'{}' $T/'{}'
(cd $S && find . -type f -name "maps.archive" -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72archive_to_acsii $S/'{}' $T/'{}'

# *.archive::*.bind
(cd $S && find . -type f -name "*.archive" -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72archive_bind_to_ascii $S/'{}' $T/'{}'

echo "Notice"

cat > $T/README.txt <<NOTICE
                                          -----------------------------------------------------------


Historical Background

This edition of the Multics software materials and documentation is provided and donated
to Massachusetts Institute of Technology by Group BULL including BULL HN Information Systems Inc. 
as a contribution to computer science knowledge.  
This donation is made also to give evidence of the common contributions of Massachusetts Institute of Technology,
Bell Laboratories, General Electric, Honeywell Information Systems Inc., Honeywell BULL Inc., Groupe BULL
and BULL HN Information Systems Inc. to the development of this operating system. 
Multics development was initiated by Massachusetts Institute of Technology Project MAC (1963-1970),
renamed the MIT Laboratory for Computer Science and Artificial Intelligence in the mid 1970s, under the leadership
of Professor Fernando Jose Corbato. Users consider that Multics provided the best software architecture 
for managing computer hardware properly and for executing programs. Many subsequent operating systems 
incorporated Multics principles.
Multics was distributed in 1975 to 2000 by Group Bull in Europe , and in the U.S. by Bull HN Information Systems Inc., 
as successor in interest by change in name only to Honeywell Bull Inc. and Honeywell Information Systems Inc. .

                                          -----------------------------------------------------------

Permission to use, copy, modify, and distribute these programs and their documentation for any purpose and without
fee is hereby granted,provided that the below copyright notice and historical background appear in all copies
and that both the copyright notice and historical background and this permission notice appear in supporting
documentation, and that the names of MIT, HIS, BULL or BULL HN not be used in advertising or publicity pertaining
to distribution of the programs without specific prior written permission.
    Copyright 1972 by Massachusetts Institute of Technology and Honeywell Information Systems Inc.
    Copyright 2006 by BULL HN Information Systems Inc.
    Copyright 2006 by Bull SAS
    All Rights Reserved
NOTICE

echo "chmod ..."

chmod -w -R $T
