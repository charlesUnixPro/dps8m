#!/bin/sh

#sed -n < system_book_12_3.ascii '/Begin collection/,/\(^End coll\)\|\(^Coll\)]/p'
sed -n < system_book_12_3.ascii \
  '/Begin collection [012]/,/[Cc]ollection/{
     /^[a-z]/,+2 p
   }' | \
awk '{ if (substr ($2, 1, 1) == "(") { getline; getline; next; }
       segname = $1; segno = strtonum (0$2); 
       R = match ($0, "\\<read\\>") != 0;
       E = match ($0, "\\<execute\\>") != 0;
       W = match ($0, "\\<write\\>") != 0;
       P = match ($0, "\\<privileged\\>") != 0;
       R1 = substr ($3, 2, 1);
       R2 = substr ($4, 1, 1);
       R3 = substr ($5, 1, 1);
       # print "1:" $0;
       getline; 
       # print "2:" $0;
       getline; 
       # print "3:" $0;
       n = match ($0, "\\<path\\>:");
       if (n) path = "\"" substr ($0, n + 6) "\""; else path = "NULL";
       printf "    {\"%s\", 0%05o, %d, %d, %d, %d, %d, %d, %d, %s},\n", segname, segno, R, E, W, P, R1, R2, R3, path;
       # print "";
     }' >slte.inc 

