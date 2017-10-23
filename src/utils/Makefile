include ../Makefile.mk

all : prt2pdf$(EXE)

prt2pdf$(EXE) : prt2pdf.o
	@echo LD prt2pdf$(EXE)
	@$(LD)  $(LDFLAGS) -o prt2pdf$(EXE) prt2pdf.o 

clean :
	-rm prt2pdf$(EXE) prt2pdf.o
