The tape files are in "SIMH" format which tracks tape record sizes and tape 
marks.

A Multics Standard Tape specifies standard block sizes and a tapemark usage 
pattern which collects blocks into file-like structure.

The dumpTape program displays the contents of a SIMH tape in 36 bit octal
and 9 bit character formats

    Usage: dupmTape tape_file_name



The program 'extract_tape_files' reads a SIMH tape and produces a set of files
containing the MST files.

     Usage: extract_tape_files tape_file_name directory_to_extract_to

     $ extract_tape_files 20186.tap .
     Processing segment 1...
     Installation ID: Bull HN, Phoenix AZ, System-M
     Tape Reel ID:    12.5LDD_STANDARD_CF0019_1
     Volume Set ID:
     4680 bytes written
     Processing segment 2...
     37355760 bytes written
     Processing segment 3...
     4680 bytes written
     Processing segment 4...
     Empty segment
     Processing segment 5...
     End of tape
     15846 blocks read
     $ ls -l 20186*
       37143284 20186.tap
           4680 20186.tap.00000001.dat
       37355760 20186.tap.00000002.dat
           4680 20186.tap.00000003.dat

extract_tape_files has created 3 files; the tape label, a MST tape file, and
and the end label.

The 'mfile' program tries to identify the .dat file contents, like the Unix 
'file' command.

    Usage: mfile file [file...]

    $ mfile 20186.tap.00000002.dat 
    ==================================
    20186.tap.00000002.dat
      mst header  rec_num      0  file_num      1  nbits  18432  admin 0
      Multics backup format

mfile identifies it as a Multics backup format file.

The 'restore' program will restore the segments in the backup as files.
(Forward slashes ('/') in segment names will be placed with '+' signs)
The program also examines the start of the file, and if it seems to be
an ASCII file, it will also create a copy of the segment transcribed to
ASCII with ".ascii" appended to the file name.

    Usage: extract_tape_files .dat_file_name directory_to_extract_to


    $ restore 20186.tap.00000002.dat .
           0 DIR        >library_dir_dir>MR12.5>12.5.LDD
           0 NDC dirlst >library_dir_dir>MR12.5>12.5.LDD>
       52848 SEG        >library_dir_dir>MR12.5>12.5.LDD>ask_
    6606 bytes written; 52848 bits,1468.0 word36, 5872.0 word9
      109764 SEG        >library_dir_dir>MR12.5>12.5.LDD>ask_.pl1
    13721 bytes written; 109768 bits,3049.1 word36, 12196.4 word9

     ....

        5796 SEG        >library_dir_dir>MR12.5>12.5.LDD>nothing
    725 bytes written; 5800 bits,161.1 word36, 644.4 word9
       10557 SEG        >library_dir_dir>MR12.5>12.5.LDD>nothing.alm
    1320 bytes written; 10560 bits,293.3 word36, 1173.3 word9
    106 files restored

    $ ls library_dir_dir/MR12.5/12.5.LDD/
    ask_                              bound_misc_commands_.archive 
    ask_.pl1                          bound_misc_commands_.s.archive 
    ask_.pl1.ascii                    bound_misc_translatrs_.archive 
    bound_absentee_com_.archive       bound_misc_translatrs_.s.archive 
    ...
    bound_mail_system_.2.s.archive    nothing 
    bound_menu_.archive               nothing.alm 
    bound_menu_.s.archive             nothing.alm.ascii 

The .dat file may contain a collection of files as file 1, 2, 3, etc.
The 'scan' program will display the raw contents of the mst record headers
to help in analyzing the contents of a tape.

    Usage: scan .dat_file_name

    $ scan 20184.tap.00000002.dat ; this is from the 20184 boot tape.
         blk    rec#   file#   #bits     len     flags ver repeat
           1       0       1   36864   36864         0   1    0
           2       1       1   36864   36864         0   1    0
           3       2       1   36864   36864         0   1    0
           4       3       1   36864   36864         0   1    0
           5       4       1   36864   36864         0   1    0
           6       5       1   36864   36864         0   1    0
           7       6       1   36864   36864         0   1    0
    ....
         127     126       1   36864   36864         0   1    0
         128     127       1   36864   36864         0   1    0
         blk    rec#   file#   #bits     len     flags ver repeat
         129     127       1   36864   36864         0   1    0
         130       0       2   36864   36864         0   1    0
         131       1       2   36864   36864         0   1    0
    ....
        1129      96       9   36864   36864         0   1    0
        1130      97       9   36864   36864         0   1    0
        1131      98       9   36864   36864         0   1    0
        1132      99       9   36864   36864         0   1    0
        1133     100       9   36864   36864         0   1    0
        1134     101       9   36864   36864         0   1    0
        1135     102       9   18864   36864       640   1    0
                 flags: set padded

This shows that the file contains 9 MST files, the first being 128
records long, the last 103 records. The last block contains the flag
'set' indicating that there is another tape with additional data.


