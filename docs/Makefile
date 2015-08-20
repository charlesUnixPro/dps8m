all : ImplementationNotes.pdf EmulatorDocumentation.pdf

ImplementationNotes.pdf : ImplementationNotes.tex
	latex ImplementationNotes.tex
	dvipdf ImplementationNotes.dvi

EmulatorDocumentation.pdf : EmulatorDocumentation.tex
	latex EmulatorDocumentation.tex
	dvipdf EmulatorDocumentation.dvi

clean :
	-rm ImplementationNotes.dvi ImplementationNotes.pdf ImplementationNotes.aux ImplementationNotes.log EmulatorDocumentation.dvi EmulatorDocumentation.pdf EmulatorDocumentation.aux EmulatorDocumentation.log
