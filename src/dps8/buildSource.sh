#!/bin/bash

T=./source/
S=./MR12.3/

# *.alm, *.pl1
(cd $S && find . -type f \( -name "*.alm" -o -name "*.pl1" -o -name "*.info" -o -name "*.cds" \)  -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72_to_acsii $S/'{}' $T/'{}'

# *.s.archive
(cd $S && find . -type f -name "*.s.archive" -print0) | xargs -0 -n 1 -I{} ../tapeUtils/p72archive_to_acsii $S/'{}' $T/'{}'

# info
