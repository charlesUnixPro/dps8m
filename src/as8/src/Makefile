C_SRCS += \
./ExprEval.c \
./as.c \
./asUtils.c \
./asMisc.c \
../../dps8/dps8_opcodetable.c \
./main.c \
./pseudoOps.c 

OBJS += \
./ExprEval.o \
./as.o \
./asUtils.o \
./asMisc.o \
../../dps8/dps8_opcodetable.o \
./main.o \
./pseudoOps.o
 
CFLAGS = -I../../dps8 -I../../simhv39-0 -std=c99

all: as8

# Tool invocations
as8: $(OBJS) 
	gcc  -o "as8" $(OBJS) 
	
# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) as8
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

