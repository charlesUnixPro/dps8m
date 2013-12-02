C_SRCS += \
./as8.l \
./as8.y \
./as.c \
./asUtils.c \
./asMisc.c \
./expr.c \
./opcodeTable.c \
./opCodes.c \
./literals.c \
./main.c \
./pseudoOps.c 

OBJS += \
./as.o \
./asUtils.o \
./asMisc.o \
./expr.o \
./opcodeTable.o \
./opCodes.o \
./literals.o \
./main.o \
./pseudoOps.o \
./y.tab.o \
./lex.yy.o

CFLAGS = -std=c99

YACC = bison
LEX = flex

# rules

all: as8+ 

# Tool invocations
as8+: $(OBJS) y.tab.o lex.yy.o 
	gcc  -o "as8+" $(OBJS) 

opcodeTable.o: y.tab.h

y.tab.o: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c

lex.yy.o: lex.yy.c y.tab.h
	$(CC) $(CFLAGS) -Wno-unused -c lex.yy.c

y.tab.c: as8.y
	$(YACC) -ydv as8.y

y.tab.h: y.tab.c

lex.yy.c: as8.l
	$(LEX) -i as8.l	
	
	
# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) y.tab.c y.tab.h lex.yy.c y.output as8+
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

