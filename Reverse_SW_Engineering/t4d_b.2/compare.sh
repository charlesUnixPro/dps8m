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

if [ "${N}" == "1" -o "${N}" == "2" ]; then
  ORIGIN=030
else
  ORIGIN=010000
fi

if [ ${N} -gt 9 ]; then
  NN=${N}
else
  NN=0${N}
fi

top=../..
tu=${top}/src/tapeUtils/
as8=${top}/src/as8+/

# Preprocess
${as8}/as8pp < blk${N}.as8 > blk${N}.s.tmp

# Assemble
${as8}/as8+ blk${N}.s.tmp -o blk${N}.oct

# Pack
${tu}/pack blk${N}.oct blk${N}.pck.tmp

# Compare original tape blk to pack
cmp ${tu}/t4d_b.2/t4d_b.2.tap.000000${NN}.dat blk${N}.pck.tmp

[ "$?" != "0" ] && ( \
${tu}/unpack $ORIGIN ${tu}/t4d_b.2/t4d_b.2.tap.000000${NN}.dat blk${N}.oct.tmp; \
${tu}/unpack $ORIGIN blk${N}.pck.tmp blk${N}.pck.oct.tmp; \
xxdiff blk${N}.oct.tmp blk${N}.pck.oct.tmp \
)
