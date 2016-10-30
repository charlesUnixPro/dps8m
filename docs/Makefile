# Copyright 2015-2016 by Charles Anthony
#
# All rights reserved.
#
# This software is made available under the terms of the
# ICU License -- ICU 1.8.1 and later. 
# See the LICENSE file at the top-level directory of this distribution and
# at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE

all : ImplementationNotes.pdf EmulatorDocumentation.pdf

ImplementationNotes.pdf : ImplementationNotes.tex
	latex ImplementationNotes.tex
	dvipdf ImplementationNotes.dvi

EmulatorDocumentation.pdf : EmulatorDocumentation.tex
	latex EmulatorDocumentation.tex
	dvipdf EmulatorDocumentation.dvi

clean :
	-rm ImplementationNotes.dvi ImplementationNotes.pdf ImplementationNotes.aux ImplementationNotes.log EmulatorDocumentation.dvi EmulatorDocumentation.pdf EmulatorDocumentation.aux EmulatorDocumentation.log
