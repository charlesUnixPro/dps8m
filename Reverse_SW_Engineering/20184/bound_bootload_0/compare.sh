#!/bin/bash
  ORIGIN=024000

top=../../../
tu=${top}/src/tapeUtils/
as8=${top}/src/as8+/

# Preprocess
${as8}/as8pp < bound_bootload_0.as8 > bound_bootload_0.s.tmp

# Assemble
${as8}/as8+ bound_bootload_0.s.tmp -o bound_bootload_0.oct

# Pack
${tu}/pack bound_bootload_0.oct bound_bootload_0.pck.tmp

# Compare original tape blk to pack
cmp bound_bootload_0.dat bound_bootload_0.pck.tmp

[ "$?" != "0" ] && ( \
${tu}/unpack $ORIGIN bound_bootload_0.dat bound_bootload_0.oct.tmp
${tu}/unpack $ORIGIN bound_bootload_0.pck.tmp bound_bootload_0.pck.oct.tmp
xxdiff bound_bootload_0.oct.tmp bound_bootload_0.pck.oct.tmp \
)
