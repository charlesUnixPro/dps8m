
%{
    #define YYDEBUG 1

    #include <stdio.h>
    #include <stdlib.h>
    
    #include "as.h"
    
    extern int yylineno;
    extern char* yytext;

    extern bool bTermCond;
    extern bool bReg;
    extern bool bvd;
    
    int yyerror(const char* msg);
//    void yyerror(char *s, ...);
    int yyErrorCount = 0;
    void lexSetState(int s);
    
    char temp[256];
    
    word36 getValue(char *s);
    
    void popAndReset();
    void setOmode();
    
    tuple *newTuple();
    list *newList();
    
    //yydebug = 1;
    
%}

%union
{
	word36s        i;
    word72       i72;
	char          *s;      /* for character pointers       */
    char           c;
    long double    r;
    opCode        *o;
    pseudoOp      *p;
    struct opnd *opnd;
    symtab        *y;  /* for SYMBOl entries */
    list        *lst;  /* for lists of things ..... */
    literal     *lit;  /* for literals */
    tuple         *t;  /* pointer to a tuple for vfd, etc.... */
 }


%verbose
%error-verbose

%start input

%token LABEL SYMBOL PSEUDOOP OCT SEGDEF VFD STROP PSEUDOOP2 DEC DESC DESC2 PSEUDOOPD2 BOOL EQU BSS
%token DECIMAL OCTAL HEX STRING AH REG NAME CALL SAVE RETURN TALLY ARG ZERO ORG ITS ITP OCTLIT DECLIT DECLIT2 NULLOP MOD
%token OPCODE OPCODEMW OPCODERPT OPCODEARS OPCODESTC
%token L To Ta Th TERMCOND
%token SINGLE DOUBLE SGLLIT DBLLIT ITSLIT ITPLIT VFDLIT DOUBLEINT

%type <s> SYMBOL STRING LABEL TERMCOND
%type <p> PSEUDOOP STROP OCT VFD PSEUDOOP2 SEGDEF DEC DESC DESC2 PSEUDOOPD2 BSS TALLY ITS ITP
%type <i> DECIMAL OCTAL HEX integer expr lexpr ptr_reg modifier L operand BOOL EQU REG rexpr OCTLIT DECLIT arg
%type <i72> DOUBLEINT DECLIT2 
%type <c> AH Ta Th To
%type <r> SINGLE DOUBLE SGLLIT DBLLIT
%type <o> OPCODE OPCODEMW OPCODERPT OPCODEARS OPCODESTC
%type <lst> symlist exprlist lexprlist optexplist optarglist optintlist opterrlist decs declist
%type <lit> literal
%type <t> vfdArg vfdArgs mfk mfks eismf eismfs eisopt rptlst


/*%right '='
%right '?' ':'
%left NOT
%left OR
%left AND
%left EQ NE
%left LT LE GT GE*/
%left '+' '-'		/* plus and minus have the least priority	*/
%left '*' '/' '%' 	/* multiply, divide and modulus are next	*/
%left NEG NOT		/* negate/complement is next				*/

%%
input: /* empty  */
    | input line 
    ;

expr: expr '+' expr         { $$ = $1 + $3;         }
    | expr '-' expr         { $$ = $1 - $3;         }
    | expr '*' expr         { $$ = $1 * $3;         }
    | expr '/' expr         { $$ = $1 / $3;         }
    | expr '%' expr         { $$ = $1 % $3;         }
    | '(' expr ')'          { $$ = $2;              }
    | '-' expr %prec NEG 	{ $$ = -$2;             }
    | SYMBOL                { $$ = getValue($1);    }
    | integer
    | '*'                   { $$ = (word36s)addr;   }
    ;

lexpr: lexpr '+' lexpr         { $$ = $1 | $3; }
     | lexpr '-' lexpr         { $$ = $1 ^ $3; }
     | lexpr '*' lexpr         { $$ = $1 & $3; }
     | lexpr '/' lexpr         { $$ = $1 & ~$3;} 
     | '(' lexpr ')'           { $$ = $2;      }
     | '^' lexpr %prec NOT 	   { $$ = ~$2;     }
     | '-' expr %prec NEG 	   { $$ = $2 ^ 0400000000000LL; /* flip the sign bit */ }
     | SYMBOL                  { $$ = getValue($1); }
     | OCTAL
     | '*'                     { $$ = (word36s)addr; }
     ;

integer
    : DECIMAL
    | OCTAL
    | HEX
    ;

optintlist: /* empty */      { $$ = NULL;   }
    |                integer { $$ = NULL; list *n = newList(); n->i36 = $1; DL_APPEND($$, n); }
    | optintlist ',' integer { $$ = $1;   list *n = newList(); n->i36 = $3; DL_APPEND($1, n); }
    ;

line : labels stmt eol
     | error '\n'
     ;

labels : /* empty */
    | label
    | labels label
    ;

label: LABEL /*if (add_label($1) != 0) { YYERROR; } */
     ;

stmt: /* empty */
    | instr
    | pop
    ;

instr   : OPCODE        operands                                        { opnd.o = $1; doOpcode(&opnd);  }
        | OPCODEMW      mfks                                            { doMWEis($1, $2);          }

        | OPCODEARS     ptr_reg '|' operand                             { doARS($1, $2, $4,  0);    }
        | OPCODEARS     ptr_reg '|' operand ',' modifier                { doARS($1, $2, $4, $6);    }
        | OPCODEARS                 operand ',' modifier ',' expr       { doARS($1, $6, $2, $4);    }

        | OPCODERPT     expr ',' expr ','   {bTermCond = true;} rptlst  { doRPT($1, $2, $4, $7);    }
        | OPCODERPT          ',' expr                                   { doRPT($1, 0, $3, NULL);   }
        | OPCODERPT     expr          ','   {bTermCond = true;} rptlst  { doRPT($1, 0, $2, $5);     }

        | OPCODESTC                 operand ',' {setOmode();} lexpr     { doSTC($1, $2, $5, -1);         }
        | OPCODESTC     ptr_reg '|' operand ',' {setOmode();} lexpr     { doSTC($1, $4, $7, (int)$2);    }
        ;

rptlst: /* empty */                         { $$ = NULL;    }
      |            TERMCOND                 { $$ = NULL; tuple *t = newTuple(); t->a.p = $1;  DL_APPEND($$, t);}
      | rptlst ',' TERMCOND                 { $$ = $1;   tuple *t = newTuple(); t->a.p = $3;  DL_APPEND($1, t);}
      ;

operand :             expr
        ;

operands: /* empty */                       { opnd.hi = 0; opnd.lo = 0;                     }
        |             operand               { opnd.hi = $1 & AMASK; opnd.lo = 0;            }
        |             operand ',' modifier  { opnd.hi = $1 & AMASK; opnd.lo = $3 & 077;     }
        |             literal               { opnd.hi = $1->addr & AMASK; opnd.lo = 0;      }
        |             literal ',' modifier  { if ($3 == 3 || $3 == 7)
                                                  opnd.hi = get18($1, (int)$3); // process literal w/ du/dl modifier
                                              else
                                                  opnd.hi = $1->addr & AMASK;
                                              opnd.lo = $3 & 077;
                                            }
        | ptr_reg '|' operand               { opnd.bit29 = true; opnd.hi = (word18)(($1 << 15) | ($3 & 077777)); opnd.lo = 0; }
        | ptr_reg '|' operand ',' modifier  { opnd.bit29 = true; opnd.hi = (word18)(($1 << 15) | ($3 & 077777)); opnd.lo = $5 & 077; }
        | external
        | external ',' modifier
        | VFDLIT    vfdArgs                 { literal *l = doVFDLiteral($2); opnd.hi = l->addr & AMASK; opnd.lo = 0;    }
        ;

ptr_reg : SYMBOL {
                    $$ = -1;
                    if (nPass == 2)
                    {
                        int npr = getPRn($1);
                        if (npr == -1)
                        {
                            symtab *s = getsym($1);
                            if (!s)
                            {
                                sprintf(temp, "invalid PR <%s>", $1);
                                yyerror(temp);
                                YYERROR;
                            }
                            npr = s->value & 7; // only keep lower 3 bits of symbol value
                        }
                        $$ = npr;
                    }
                 }
        | integer { $$ = $1 & 07; }
        ;

external : SYMBOL '$' SYMBOL
         ;

modifier: SYMBOL        { $$ = getmod($1);  }
        | '*'           { $$ = getmod("*"); }
        | '*' SYMBOL    { strcpy(temp, "*"); strcat(temp,  $2);  $$ = getmod(temp);  }
        | SYMBOL '*'    { strcpy(temp,  $1); strcat(temp, "*");  $$ = getmod(temp);  }
        | integer       { sprintf(temp, "%lld", $1); $$ = getmod(temp);                }
        | '*' integer   { sprintf(temp, "*%lld", $2); $$ = getmod(temp);               }
        | integer '*'   { sprintf(temp, "%lld*", $1); $$ = getmod(temp);               }
        ;

// for MF EIS
mfk : '(' eismfs ')'        { $$ = newTuple(); $$->a.c = 'm'; $$->b.t =  $2; }
    |     eisopt            { $$ = newTuple(); $$->a.c = 'o'; $$->b.t =  $1; }
    ;

mfks: /* empty */           { $$ = NULL;                    }
    |          mfk          { $$ = NULL; DL_APPEND($$, $1); }
    | mfks ',' mfk          { $$ = $1;   DL_APPEND($1, $3); }
    ;
    
eismfs
    : /* empty */           { $$ = NULL;                    }
    | eismf                 { $$ = NULL; DL_APPEND($$, $1); }
    | eismfs ',' eismf      { $$ = $1;   DL_APPEND($1, $3); }
    ;

eismf
    : SYMBOL                { $$ = newTuple(); $$->a.p = $1;  }
    ;

eisopt
    : SYMBOL                 { $$ = newTuple(); $$->a.c = '1'; $$->b.p = $1;                 }
    | SYMBOL '(' lexpr ')'   { $$ = newTuple(); $$->a.c = '2'; $$->b.p = $1;  $$->c.i36 = $3;}
    ;


symlist:             SYMBOL { $$ = NULL; list *n = newList(); n->p = $1; DL_APPEND($$, n); }
       | symlist ',' SYMBOL { $$ = $1;   list *n = newList(); n->p = $3; DL_APPEND($1, n); }
       ;


exprlist:              expr { $$ = NULL; list *n = newList(); n->i36 = $1; DL_APPEND($$, n); }
        | exprlist ',' expr { $$ = $1;   list *n = newList(); n->i36 = $3; DL_APPEND($1, n); }
        | exprlist ','      { $$ = $1;   list *n = newList(); n->i36 =  0; DL_APPEND($1, n); }
        ;

lexprlist:               lexpr { $$ = NULL; list *n = newList(); n->i36 = $1; DL_APPEND($$, n); }
         | lexprlist ',' lexpr { $$ = $1;   list *n = newList(); n->i36 = $3; DL_APPEND($1, n); }
         | lexprlist ','       { $$ = $1;   list *n = newList(); n->i36 =  0; DL_APPEND($1, n); }
         ;

decs:  expr     { $$ = newList(); $$->i36 = $1;  $$->whatAmI = lstI36;    }
    | SINGLE    { $$ = newList(); $$->r = $1;    $$->whatAmI = lstSingle; }
    | DOUBLE    { $$ = newList(); $$->r = $1;    $$->whatAmI = lstDouble; }
    | DOUBLEINT { $$ = newList(); $$->i72 = $1;  $$->whatAmI = lstI72;    }
    ;

declist
    : decs              { $$ = NULL; DL_APPEND($$, $1);  }
    | declist ',' decs  { $$ = $1;   DL_APPEND($$, $3);  }
    ;

/* add literal to optexplist */
optexplist: /* empty */     { $$ = NULL; }
    | exprlist              { $$ = $1;   }
    ;

optarglist: /* empty */     { $$ = NULL;    }
    |                arg    { $$ = NULL; list *n = newList(); n->i36 = $1; DL_APPEND($$, n);  }
    | optarglist ',' arg    { $$ = $1;   list *n = newList(); n->i36 = $3; DL_APPEND($1, n);  }
    ;

arg : expr
    | literal   { $$ = $1->addr;    }
    ;

literal
    : '=' DECIMAL AH STRING                         { $$ = doStringLiteral((int)$2, $3, $4); }
    | '='         AH STRING                         { $$ = doStringLiteral(      0, $2, $3); }
    | OCTLIT                                        { $$ = doNumericLiteral( 8, $1);         }
    | DECLIT                                        { $$ = doNumericLiteral(10, $1);         }
    | SGLLIT                                        { $$ = doFloatingLiteral(1, $1);         }
    | DBLLIT                                        { $$ = doFloatingLiteral(2, $1);         }
    | ITPLIT '(' ptr_reg ',' expr ')'               { $$ = doITSITPLiteral(041, $3, $5,  0); }
    | ITPLIT '(' ptr_reg ',' expr ',' modifier ')'  { $$ = doITSITPLiteral(041, $3, $5, $7); }
    | ITSLIT '(' expr ',' expr ')'                  { $$ = doITSITPLiteral(043, $3, $5,  0); }
    | ITSLIT '(' expr ',' expr ',' modifier ')'     { $$ = doITSITPLiteral(043, $3, $5, $7); }
    | VFDLIT '(' vfdArgs ')'                        { $$ = doVFDLiteral($3); popAndReset();  }
    | DECLIT2                                       { $$ = doNumericLiteral72($1);           }
    ;


vfdArgs
    :             vfdArg    { $$ = NULL; DL_APPEND($$, $1);          }
    | vfdArgs ',' vfdArg    { $$ = $1;   DL_APPEND($1, $3);          }
    ;

vfdArg:    L '/' expr       { $$ = newTuple(); $$->a.c =  0; $$->b.i = (int)$1; $$->c.i36 = $3;    }
      | Ta L '/' STRING     { $$ = newTuple(); $$->a.c = $1; $$->b.i = (int)$2; $$->c.p   = $4;    }
      | Th L '/' STRING     { $$ = newTuple(); $$->a.c = $1; $$->b.i = (int)$2; $$->c.p   = $4;    }
      | To L '/' lexpr      { $$ = newTuple(); $$->a.c = $1; $$->b.i = (int)$2; $$->c.i36 = $4;    }
      | L Ta '/' STRING     { $$ = newTuple(); $$->b.i = (int)$1; $$->a.c = $2; $$->c.p   = $4;    }
      | L Th '/' STRING     { $$ = newTuple(); $$->b.i = (int)$1; $$->a.c = $2; $$->c.p   = $4;    }
      | L To '/' lexpr      { $$ = newTuple(); $$->b.i = (int)$1; $$->a.c = $2; $$->c.i36 = $4;    }
      ;

rexpr:  REG
     | expr 
     ;


pop: PSEUDOOP                                                            { doPop0($1);   }
   | PSEUDOOP2      operands


   | ZERO                   ',' expr                                     { doZero(0, $3);           }
   | ZERO                   ',' literal                                  { doZero(0, $3->addr);     }
   | ZERO           literal ',' expr                                     { doZero($2->addr, $4);    }
   | ZERO           literal ',' literal                                  { doZero($2->addr, $4->addr);}
   | ZERO           expr    ',' literal                                  { doZero($2, $4->addr);    }
   | ZERO           literal                                              { doZero($2->addr, 0);     }
   | ZERO           expr    ',' expr                                     { doZero($2, $4);          }
   | ZERO           expr                                                 { doZero($2, 0);           }
   | ZERO               VFDLIT vfdArgs                                   { literal *l = doVFDLiteral($3); doZero(l->addr,0);  popAndReset();  }
   | ZERO           ',' VFDLIT vfdArgs                                   { literal *l = doVFDLiteral($4); doZero(0,l->addr);  popAndReset();  }

   | ARG            operands                                             { doArg(&opnd);  }
   | NAME           SYMBOL
   | ORG            expr                                                 { doOrg($2);     }
   | MOD            expr                                                 { doMod($2);     }

   | SEGDEF         symlist
   | OCT            lexprlist                                            { doOct($2);  }
   | DEC            declist                                              { doDec($2);  }
   | DESC                       expr                                     { doDescriptor($1, $2,  0,  0,  0,      -1); }
   | DESC                       expr '(' expr ')'                        { doDescriptor($1, $2, $4,  0,  0,      -1); }
   | DESC           ptr_reg '|' expr              ',' rexpr              { doDescriptor($1, $4,  0, $6,  0, (int)$2); }
   | DESC                       expr '(' expr ')' ',' rexpr              { doDescriptor($1, $2, $4, $7,  0,      -1); }
   | DESC                       expr              ',' rexpr              { doDescriptor($1, $2,  0, $4,  0,      -1); }
   | DESC           ptr_reg '|' expr '(' expr ')' ',' rexpr              { doDescriptor($1, $4, $6, $9,  0, (int)$2); }
   | DESC           ptr_reg '|' expr              ',' rexpr ',' expr     { doDescriptor($1, $4,  0, $6, $8, (int)$2); }
   | DESC                       expr '(' expr ')' ',' rexpr ',' expr     { doDescriptor($1, $2, $4, $7, $9,      -1); }
   | DESC                       expr              ',' rexpr ',' expr     { doDescriptor($1, $2,  0, $4, $6,      -1); }
   | DESC           ptr_reg '|' expr '(' expr ')' ',' rexpr ',' expr     { doDescriptor($1, $4, $6, $9, $11,(int)$2); }
   | DESC2          exprlist                                             { doDescriptor2($1, $2);                     }
   | PSEUDOOPD2     symlist

   | STROP          STRING                  { doStrop($1, $2, 0);       }
   | STROP          STRING  ',' expr        { doStrop($1, $2, (int)$4); }

   | VFD            vfdArgs                 { doVfd($2);                }

   | BOOL           SYMBOL ',' lexpr        { doBoolEqu($2, $4);        }
   | EQU            SYMBOL ','  expr        { doBoolEqu($2, $4);        }

   | BSS            SYMBOL ','  expr        { doBss($2,   $4);          }
   | BSS                   ','  expr        { doBss(NULL, $3);          }
   | BSS                        expr        { doBss(NULL, $2);          }

   | CALL           SYMBOL                                            { doCall($2,  0, NULL, NULL); }
   | CALL           SYMBOL ',' modifier                               { doCall($2, $4, NULL, NULL); }
   | CALL           SYMBOL ',' modifier '(' optarglist ')' opterrlist { doCall($2, $4,   $6,   $8); }
   | CALL           SYMBOL              '(' optarglist ')' opterrlist { doCall($2,  0,   $4,   $6); }

   | SAVE           optintlist                                        { doSave ($2);                }

   | RETURN         SYMBOL                                            { doReturn($2, 0);            }
   | RETURN         SYMBOL ',' integer                                { doReturn($2, $4);           }

   | TALLY          exprlist                                          { doTally($1, $2);            }

   | ITP            ptr_reg ',' expr                { doITSITP($1, $2, $4,  0);    }
   | ITP            ptr_reg ',' expr ',' modifier   { doITSITP($1, $2, $4, $6);    }
   | ITS            expr ',' expr                   { doITSITP($1, $2, $4,  0);    }
   | ITS            expr ',' expr ',' modifier      { doITSITP($1, $2, $4, $6);    }

   | NULLOP
   ;
        
opterrlist: /* empty */         { $$ = NULL;                                }
    | exprlist '\'' SYMBOL '\'' { $$ = newList(); $$->l = $1; $$->p = $3;   } /* change to exprlist */
    |          '\'' SYMBOL '\'' { $$ = newList(); $$->l = NULL; $$->p = $2; }
    | exprlist                  { $$ = newList(); $$->l = $1; $$->p = NULL; }
    ;

eol: '\n'
| ';'
;


%%
 /*--------------------------------------------------------*/
 /* Additional C code */

struct yyList
{
    YYSTYPE yy;
    
    struct yyList *prev;
    struct yyList *next;
};
typedef struct yyList yyList;


/* Error processor for yyparse */
#include <stdio.h>
int yyerror(const char* msg)
{
    if (FILEsp == 0)
        fprintf(stderr, "Error on line %d: %s near token '%s'\n", yylineno, msg, yytext);
    else
        fprintf(stderr, "Error on line %d in %s: %s near token '%s'\n", yylineno, LEXCurrentFilename(),msg, yytext);
    yyErrorCount += 1;
    fprintf(stderr, "%s\n", LEXline);
    return 0;
}

void yyprintf(const char *fmt, ...)
{
    char p[1024];
    va_list ap;
    va_start(ap, fmt);
    (void) vsnprintf(p, 1024, fmt, ap);
    va_end(ap);
    yyerror(p);
    
    //yynerrs += 1;
}


struct PRtab {
    char *alias;    ///< pr alias
    int   n;        ///< number alias represents ....
} _prtab[] = {
    
    {"pr0", 0}, ///< pr0 - 7
    {"pr1", 1},
    {"pr2", 2},
    {"pr3", 3},
    {"pr4", 4},
    {"pr5", 5},
    {"pr6", 6},
    {"pr7", 7},
    
    // from: ftp://ftp.stratus.com/vos/multics/pg/mvm.html
    {"ap",  0},
    {"ab",  1},
    {"bp",  2},
    {"bb",  3},
    {"lp",  4},
    {"lb",  5},
    {"sp",  6},
    {"sb",  7},
    
    {0,     0}
    
};

int getPRn(char *s)
{
    for(int n = 0 ; _prtab[n].alias; n++)
        if (strcasecmp(_prtab[n].alias, s) == 0)
            return _prtab[n].n;
    
    return -1;
}

word36 getValue(char *s)
{
    symtab *y = getsym(s);

    if (!y)
    {
        if (nPass == 1)
            return -1;
        
        yyprintf("undefined symbol <%s>", s);
        return 0;
    }
    return y->value;
}

tuple *newTuple()
{
    return calloc(1, sizeof(tuple));
}

list *newList()
{
    list *l = calloc(1, sizeof(list));
    l->whatAmI = lstUnknown;
    
    return l;
}


//=============================================================================


