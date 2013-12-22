#!/bin/bash
#gpp -C < blk0.as8 > blk0.s
#../as8+/as8+ blk0.s

#../as8+/as8+ blk0.as8
#awk < as8.oct '{ print $3}' > as8.dmp
#cmp blk0.dmp as8.dmp
#[ "$?" != "0" ] && xxdiff blk0.dmp as8.dmp

#../as8+/as8+ blk3.as8
#awk < as8.oct '{ print $3}' > as8.dmp
#cmp blk3.dmp as8.dmp
#[ "$?" != "0" ] && xxdiff blk3.dmp as8.dmp
if [ "${1}" == "" ]; then
  echo "Block number?"
  exit
fi
N=${1}

if [ "${N}" == "1" ]; then
  ORIGIN=030
else
  ORIGIN=010000
fi

if [ ${N} -gt 9 ]; then
  NN=${N}
else
  NN=0${N}
fi

#gpp -n \
#    -U "" "" "(" "," ")" "(" ")" "#" "" \
#    -M "\n#\w" "\n" " " " " "\n" "" "" \
#    +c "/*" "*/" +c "//" "\n" +c "\\\n" "" \
#    +s "\"" "\"" "\\" +s "'" "'" "\\" \
##gpp -C -c "/*" -c "//" -c "\\\n" \
#       -s "\"" -s "'" \
#        +ccss "\"" "\n" \
#       -n \
#gpp -n \
#    -U "" "" "(" "," ")" "(" ")" "#" "" \
#    -M "\n#\w" "\n" " " " " "\n" "" "" \
#    +ccss "\"" "\n" \
# < blk${N}.as8 > blk${N}.s.tmp
#sed 's/"/\/\//' < blk${N}.as8 | cpp -CC -x assembler-with-cpp | tail -n +43 | sed 's/\/\//"/' > blk${N}.s.tmp

top=..
tu=${top}/src/tapeUtils/
as8=${top}/src/as8+/

# Preprocess
${as8}/as8pp < blk${N}.as8 > blk${N}.s.tmp

# Assemble
${as8}/as8+ blk${N}.s.tmp -o blk${N}.oct

# Pack
${tu}/pack blk${N}.oct blk${N}.pck.tmp

# Compare original tape blk to pack
cmp ${tu}/tests/verify000000${NN}.pck blk${N}.pck.tmp

[ "$?" != "0" ] && ( \
${tu}/disasm ${ORIGIN} blk${N}.pck.tmp > blk${N}.pck.dis.tmp;  \
${as8}/as8+ blk${N}.pck.dis.tmp -o blk${N}.oct.dis.oct.tmp;  \
xxdiff ${tu}/tests/verify000000${NN}.oct blk${N}.oct.dis.oct.tmp )
