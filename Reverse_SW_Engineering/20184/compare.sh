#!/bin/bash
if [ "${1}" == "" ]; then
  echo "Block number?"
  exit
fi
N=${1}

if [ "${N}" == "1" ]; then
  ORIGIN=030
else
# XXX wrong for 20184
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
cmp ${tu}/20184/verify000000${NN}.pck blk${N}.pck.tmp

[ "$?" != "0" ] && ( \
${tu}/disasm ${ORIGIN} blk${N}.pck.tmp > blk${N}.pck.dis.tmp;  \
${as8}/as8+ blk${N}.pck.dis.tmp -o blk${N}.oct.dis.oct.tmp;  \
xxdiff ${tu}/tests/verify000000${NN}.oct blk${N}.oct.dis.oct.tmp )
