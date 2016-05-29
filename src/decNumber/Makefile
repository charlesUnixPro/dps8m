include ../Makefile.mk

C_SRCS = \
decContext.c \
decNumber.c 


OBJS = \
decContext.o \
decNumber.o 

CFLAGS += -Wall 

all: decNumber.a

decNumber.a : $(OBJS)
	@echo AR $@
	@$(AR) cr $@ $?

# Other Targets
clean:
	@echo CLEAN decNumber
	-@$(RM) $(OBJS) decNumber.a

$(OBJS) : decContext.h decimal128.h decNumber.h decQuad.h decDouble.h decimal32.h decNumberLocal.h decSingle.h decDPD.h decimal64.h  decPacked.h


.PHONY: all clean dependents
.SECONDARY:

