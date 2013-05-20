
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
    expr          *e;  /* for expression nodes */
 }


%verbose
%error-verbose

%start input
%token LABEL SYMBOL PSEUDOOP OCT SEGDEF SEGREF VFD STROP PSEUDOOP2 DEC DESC DESC2 PSEUDOOPD2 BOOL EQU BSS
%token DECIMAL OCTAL HEX STRING AH REG NAME CALL SAVE RETURN TALLY ARG ZERO ORG ITS ITP OCTLIT DECLIT DECLIT2 NULLOP MOD
%token OPCODE OPCODEMW OPCODERPT OPCODEARS OPCODESTC
%token L To Ta Th TERMCOND
%token SINGLE DOUBLE SGLLIT DBLLIT ITSLIT ITPLIT VFDLIT DOUBLEINT
%token SHORT_CALL SHORT_RETURN ENTRY PUSH TEMP CALLH CALLM OPTIONS INTEGER LINK INHIBIT

%type <s> SYMBOL STRING LABEL TERMCOND
%type <p> PSEUDOOP STROP OCT VFD PSEUDOOP2 SEGDEF DEC DESC DESC2 PSEUDOOPD2 BSS TALLY ITS ITP TEMP
%type <i> DECIMAL OCTAL HEX integer ptr_reg modifier L BOOL EQU REG rexpr OCTLIT DECLIT arg CALL CALLH CALLM INTEGER
%type <i72> DOUBLEINT DECLIT2
%type <c> AH Ta Th To
%type <r> SINGLE DOUBLE SGLLIT DBLLIT
%type <o> OPCODE OPCODEMW OPCODERPT OPCODEARS OPCODESTC
%type <lst> symlist exprlist lexprlist optexplist optarglist optintlist opterrlist decs declist
%type <lit> literal
%type <t> vfdArg vfdArgs mfk mfks eismf eismfs eisopt rptlst tempelement templist options option external
%type <e> expr lexpr operand optarg arg2 entry

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

expr: expr '+' expr         { $$ = add($1, $3);      }
    | expr '-' expr         { $$ = subtract($1, $3); }
    | expr '*' expr         { $$ = multiply($1, $3); }
    | expr '/' expr         { $$ = divide($1, $3);   }
    | expr '%' expr         { $$ = modulus($1, $3);  }
    | '(' expr ')'          { $$ = $2;               }
    | '-' expr %prec NEG 	{ $$ = neg($2);          }
    | SYMBOL                { $$ = exprSymbolValue($1);    }
    | integer               { $$ = exprWord36Value($1);    }
    | '*'                   { $$ = exprWord36Value((word36)addr); $$->type = eExprRelative; $$->lc = ".text.";  }
    ;

lexpr
    : lexpr '+' lexpr         { $$ = or($1, $3);    }
    | lexpr '-' lexpr         { $$ = xor($1, $3);   }
    | lexpr '*' lexpr         { $$ = and($1, $3);   }
    | lexpr '/' lexpr         { $$ = andnot($1, $3);}
    | '(' lexpr ')'           { $$ = $2;            }
    | '^' lexpr %prec NOT 	  { $$ = not($2);       }
    | '-' expr %prec NEG 	  { $$ = neg8($2);      }
    | SYMBOL                  { $$ = exprSymbolValue($1); }
    | OCTAL                   { $$ = exprWord36Value($1); }
    | '*'                     { $$ = exprWord36Value((word36)addr); $$->type = eExprRelative; $$->lc = ".text.";  }
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
    |        label
    | labels label
    ;

label: LABEL /*if (add_label($1) != 0) { YYERROR; } */
    ;

stmt: /* empty */
    | instr
    | pop
    ;

instr
    : OPCODE        operands                                        { opnd.o = $1; doOpcode(&opnd); }
    | OPCODEMW      mfks                                            { doMWEis($1, $2);              }

    | OPCODEARS     ptr_reg '|' operand                             { doARS($1, $2, $4->value,  0);        }
    | OPCODEARS     ptr_reg '|' operand ',' modifier                { doARS($1, $2, $4->value, $6);        }
    | OPCODEARS                 operand ',' modifier ',' expr       { doARS($1, $6->value, $2->value, $4); }

    | OPCODERPT     expr ',' expr ','   {bTermCond = true;} rptlst  { doRPT($1, $2->value, $4->value, $7); }
    | OPCODERPT          ',' expr                                   { doRPT($1, 0, $3->value, NULL);       }
    | OPCODERPT     expr          ','   {bTermCond = true;} rptlst  { doRPT($1, 0, $2->value, $5);         }

    | OPCODESTC                 operand ',' {setOmode();} lexpr     { doSTC($1, $2->value, $5->value, -1);        }
    | OPCODESTC     ptr_reg '|' operand ',' {setOmode();} lexpr     { doSTC($1, $4->value, $7->value, (int)$2);   }
    ;

rptlst: /* empty */                       { $$ = NULL;    }
    |            TERMCOND                 { $$ = NULL; tuple *t = newTuple(); t->a.p = $1;  DL_APPEND($$, t);}
    | rptlst ',' TERMCOND                 { $$ = $1;   tuple *t = newTuple(); t->a.p = $3;  DL_APPEND($1, t);}
    ;

operand
    : expr 
    ;

operands: /* empty */       { opnd.hi = 0; opnd.lo = 0;                        }
    | operand               {
                                opnd.lo = 0;
                                if ($1->type == eExprTemporary)
                                {
                                    opnd.hi = (6 << 15) | ($1->value & 077777);
                                    opnd.bit29 = true;
                                } else if ($1->type == eExprSegRef)
                                {
                                    opnd.hi = (4 << 15) | ($1->value & 077777);
                                    opnd.bit29 = true;
                                    opnd.lo = getmod("*");  // indirect via pr4 
                                }
                                else
                                    opnd.hi = $1->value & AMASK;

                            }
    | operand ',' modifier  {
                                opnd.lo = $3 & 077;
                                if ($1->type == eExprTemporary)
                                {
                                    opnd.hi = (6 << 15) | ($1->value & 077777);
                                    opnd.bit29 = true;
                                } else if ($1->type == eExprSegRef)
                                {
                                    opnd.hi = (4 << 15) | ($1->value & 077777);
                                    opnd.bit29 = true;
                                    opnd.lo = getmod("*");
                                }
                                else
                                    opnd.hi = $1->value & AMASK;

                            }
    | literal               { opnd.hi = $1->addr & AMASK; opnd.lo = 0;         }
    | literal ',' modifier  {
                               if ($3 == 3 || $3 == 7)
                                  opnd.hi = get18($1, (int)$3); // process literal w/ du/dl modifier
                               else
                                  opnd.hi = $1->addr & AMASK;
                               opnd.lo = $3 & 077;
                            }
    | ptr_reg '|' operand               { opnd.bit29 = true; opnd.hi = (word18)(($1 << 15) | ($3->value & 077777)); opnd.lo = 0;        }
    | ptr_reg '|' operand ',' modifier  { opnd.bit29 = true; opnd.hi = (word18)(($1 << 15) | ($3->value & 077777)); opnd.lo = $5 & 077; }

    | VFDLIT    vfdArgs                 { literal *l = doVFDLiteral($2); opnd.hi = l->addr & AMASK; opnd.lo = 0;    }

    | external
    | external ',' modifier
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

external
    :     SYMBOL     '$'     SYMBOL         { tuple *t = newTuple(); t->a.p = $1; t->b.p = $3; $$ = t;  }
    | '<' SYMBOL '>' '|' '[' SYMBOL ']'     { tuple *t = newTuple(); t->a.p = $2; t->b.p = $6; $$ = t;  }
    ;

modifier
    : SYMBOL        { $$ = getmod($1);  }
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
    |            eismf      { $$ = NULL; DL_APPEND($$, $1); }
    | eismfs ',' eismf      { $$ = $1;   DL_APPEND($1, $3); }
    | eismfs '+' eismf      { $$ = $1;   DL_APPEND($1, $3); }
    ;

eismf
    : SYMBOL                { $$ = newTuple(); $$->a.p = $1;  }
    ;

eisopt
    : SYMBOL                 { $$ = newTuple(); $$->a.c = '1'; $$->b.p = $1;                        }
    | SYMBOL '(' lexpr ')'   { $$ = newTuple(); $$->a.c = '2'; $$->b.p = $1; $$->c.i36 = $3->value; }
    ;


symlist:          SYMBOL { $$ = NULL; list *n = newList(); n->p = $1; DL_APPEND($$, n); }
    | symlist ',' SYMBOL { $$ = $1;   list *n = newList(); n->p = $3; DL_APPEND($1, n); }
    ;


exprlist:          expr { $$ = NULL; list *n = newList(); n->i36 = $1->value; DL_APPEND($$, n); }
    | exprlist ',' expr { $$ = $1;   list *n = newList(); n->i36 = $3->value; DL_APPEND($1, n); }
    | exprlist ','      { $$ = $1;   list *n = newList(); n->i36 =         0; DL_APPEND($1, n); }
    ;

lexprlist:          lexpr { $$ = NULL; list *n = newList(); n->i36 = $1->value; DL_APPEND($$, n); }
    | lexprlist ',' lexpr { $$ = $1;   list *n = newList(); n->i36 = $3->value; DL_APPEND($1, n); }
    | lexprlist ','       { $$ = $1;   list *n = newList(); n->i36 =  0; DL_APPEND($1, n); }
    ;

decs:  expr     { $$ = newList(); $$->i36 = $1->value;  $$->whatAmI = lstI36;    }
    | SINGLE    { $$ = newList(); $$->r   = $1;         $$->whatAmI = lstSingle; }
    | DOUBLE    { $$ = newList(); $$->r   = $1;         $$->whatAmI = lstDouble; }
    | DOUBLEINT { $$ = newList(); $$->i72 = $1;         $$->whatAmI = lstI72;    }
    ;

declist
    :             decs  { $$ = NULL; DL_APPEND($$, $1);  }
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

arg : expr                  { $$ = $1->value;   }
    | literal               { $$ = $1->addr;    }
    ;

literal
    : '=' DECIMAL AH STRING                         { $$ = doStringLiteral((int)$2, $3, $4); }
    | '='         AH STRING                         { $$ = doStringLiteral(      0, $2, $3); }
    | OCTLIT                                        { $$ = doNumericLiteral( 8, $1);         }
    | DECLIT                                        { $$ = doNumericLiteral(10, $1);         }
    | SGLLIT                                        { $$ = doFloatingLiteral(1, $1);         }
    | DBLLIT                                        { $$ = doFloatingLiteral(2, $1);         }
    | ITPLIT '(' ptr_reg ',' expr ')'               { $$ = doITSITPLiteral(041, $3, $5->value,  0); }
    | ITPLIT '(' ptr_reg ',' expr ',' modifier ')'  { $$ = doITSITPLiteral(041, $3, $5->value, $7); }
    | ITSLIT '(' expr ',' expr ')'                  { $$ = doITSITPLiteral(043, $3->value, $5->value,  0); }
    | ITSLIT '(' expr ',' expr ',' modifier ')'     { $$ = doITSITPLiteral(043, $3->value, $5->value, $7); }
    | VFDLIT '(' vfdArgs ')'                        { $$ = doVFDLiteral($3); popAndReset();  }
    | DECLIT2                                       { $$ = doNumericLiteral72($1);           }
    ;


vfdArgs
    :             vfdArg    { $$ = NULL; DL_APPEND($$, $1);          }
    | vfdArgs ',' vfdArg    { $$ = $1;   DL_APPEND($1, $3);          }
    ;

vfdArg:  L '/' expr       { $$ = newTuple(); $$->a.c =  0; $$->b.i = (int)$1; $$->c.i36 = $3->value;    }
    | Ta L '/' STRING     { $$ = newTuple(); $$->a.c = $1; $$->b.i = (int)$2; $$->c.p   = $4;           }
    | Th L '/' STRING     { $$ = newTuple(); $$->a.c = $1; $$->b.i = (int)$2; $$->c.p   = $4;           }
    | To L '/' lexpr      { $$ = newTuple(); $$->a.c = $1; $$->b.i = (int)$2; $$->c.i36 = $4->value;    }
    | L Ta '/' STRING     { $$ = newTuple(); $$->b.i = (int)$1; $$->a.c = $2; $$->c.p   = $4;           }
    | L Th '/' STRING     { $$ = newTuple(); $$->b.i = (int)$1; $$->a.c = $2; $$->c.p   = $4;           }
    | L To '/' lexpr      { $$ = newTuple(); $$->b.i = (int)$1; $$->a.c = $2; $$->c.i36 = $4->value;    }
    ;

rexpr
    : REG
    | expr { $$ = $1->value;    }
    ;


pop
    : OPTIONS        options                     { doOptions($2);   }

    | PSEUDOOP                                   { doPop0($1);      }
    | PSEUDOOP2      operands

    | ZERO                   ',' expr            { doZero(0, $3->value);           }
    | ZERO                   ',' literal         { doZero(0, $3->addr);            }
    | ZERO           literal ',' expr            { doZero($2->addr, $4->value);    }
    | ZERO           literal ',' literal         { doZero($2->addr, $4->addr);     }
    | ZERO           expr    ',' literal         { doZero($2->value, $4->addr);    }
    | ZERO           literal                     { doZero($2->addr, 0);            }
    | ZERO           expr    ',' expr            { doZero($2->value, $4->value);   }
    | ZERO           expr                        { doZero($2->value, 0);           }
    | ZERO           VFDLIT vfdArgs              { literal *l = doVFDLiteral($3); doZero(l->addr,0);  popAndReset();  }
    | ZERO           ',' VFDLIT vfdArgs          { literal *l = doVFDLiteral($4); doZero(0,l->addr);  popAndReset();  }

    | ARG            operands                                             { doArg(&opnd);  }
    | ORG            expr                                                 { doOrg($2->value);     }
    | MOD            expr                                                 { doMod($2->value);     }

    | OCT            lexprlist                                            { doOct($2);  }
    | DEC            declist                                              { doDec($2);  }
    | DESC                       expr                                     { doDescriptor($1, $2->value,         0,  0,  0,      -1); }
    | DESC                       expr '(' expr ')'                        { doDescriptor($1, $2->value, $4->value,  0,  0,      -1); }
    | DESC           ptr_reg '|' expr              ',' rexpr              { doDescriptor($1, $4->value,  0, $6,  0, (int)$2);        }
    | DESC                       expr '(' expr ')' ',' rexpr              { doDescriptor($1, $2->value, $4->value, $7,  0,      -1); }
    | DESC                       expr              ',' rexpr              { doDescriptor($1, $2->value,  0, $4,  0,      -1);        }
    | DESC           ptr_reg '|' expr '(' expr ')' ',' rexpr              { doDescriptor($1, $4->value, $6->value, $9,  0, (int)$2); }
    | DESC           ptr_reg '|' expr              ',' rexpr ',' expr     { doDescriptor($1, $4->value,  0, $6, $8->value, (int)$2); }
    | DESC                       expr '(' expr ')' ',' rexpr ',' expr     { doDescriptor($1, $2->value, $4->value, $7, $9->value,      -1); }
    | DESC                       expr              ',' rexpr ',' expr     { doDescriptor($1, $2->value,  0, $4, $6->value,      -1); }
    | DESC           ptr_reg '|' expr '(' expr ')' ',' rexpr ',' expr     { doDescriptor($1, $4->value, $6->value, $9, $11->value,(int)$2); }
    | DESC2          exprlist                                             { doDescriptor2($1, $2);                                   }
    | PSEUDOOPD2     symlist

    | STROP          STRING                  { doStrop($1, $2, 0);       }
    | STROP          STRING  ',' expr        { doStrop($1, $2, (int)$4); }

    | VFD            vfdArgs                 { doVfd($2);                }

    | BOOL           SYMBOL ',' lexpr        { doBoolEqu($2, $4); }
    | EQU            SYMBOL ','  expr        { doBoolEqu($2, $4); }

    | BSS            SYMBOL ','  expr        { doBss($2,   $4->value);   }
    | BSS                   ','  expr        { doBss(NULL, $3->value);   }
    | BSS                        expr        { doBss(NULL, $2->value);   }

    | CALLH          SYMBOL                                            { doHCall($2,  0, NULL, NULL); }
    | CALLH          SYMBOL ',' modifier                               { doHCall($2, $4, NULL, NULL); }
    | CALLH          SYMBOL ',' modifier '(' optarglist ')' opterrlist { doHCall($2, $4,   $6,   $8); }
    | CALLH          SYMBOL              '(' optarglist ')' opterrlist { doHCall($2,  0,   $4,   $6); }

    | SAVE           optintlist                                        { doSave ($2);                }

    | RETURN         SYMBOL                                            { doReturn($2, 0);            }
    | RETURN         SYMBOL ',' integer                                { doReturn($2, $4);           }

    | TALLY          exprlist                                          { doTally($1, $2);            }

    | ITP            ptr_reg ',' expr                { doITSITP($1, $2, $4->value,  0);         }
    | ITP            ptr_reg ',' expr ',' modifier   { doITSITP($1, $2, $4->value, $6);         }
    | ITS            expr ',' expr                   { doITSITP($1, $2->value, $4->value,  0);  }
    | ITS            expr ',' expr ',' modifier      { doITSITP($1, $2->value, $4->value, $6);  }

    | NULLOP

    | NAME           SYMBOL                          { doName($2);   }

    | CALLM          entry                                 { doMCall($2,  0, NULL); }
    | CALLM          entry ',' modifier                    { doMCall($2, $4, NULL); }
    | CALLM          entry ',' modifier '(' optarg ')'     { doMCall($2, $4,   $6); }
    | CALLM          entry              '(' optarg ')'     { doMCall($2,  0,   $4); }

    | ENTRY          symlist                         { doEntry($2);                 }
    | PUSH           expr                            { doPush($2->value);           }
    | PUSH                                           { doPush(0);                   }
    | RETURN                                         { doReturn0();                 }
    | SHORT_CALL     SYMBOL                          { doShortCall($2);             }
    | SHORT_RETURN                                   { doShortReturn();             }

    | TEMP           templist                        { doTemp($1, $2);              }

    | SEGDEF         symlist                         { doSegdef($2);                }
    | SEGREF         symlist                         { doSegref($2);                }
    
    | LINK           SYMBOL ',' external             { doLink($2, $4);              }
    
    | INHIBIT        SYMBOL                          { doInhibit($2);               }
    ;

entry
    : expr
    | ptr_reg '|' expr  { $$ = exprPtrExpr((int)$1, $3); }
    ;

templist
    :                tempelement    { $$ = NULL; DL_APPEND($$, $1);          }
    |   templist ',' tempelement    { $$ = $1;   DL_APPEND($1, $3);          }
    ;

tempelement
    : SYMBOL                    { $$ = newTuple(); $$->a.p = $1; $$->b.i36 =  1;        }
    | SYMBOL '(' ')'            { $$ = newTuple(); $$->a.p = $1; $$->b.i36 =  1;        }
    | SYMBOL '(' expr ')'       { $$ = newTuple(); $$->a.p = $1; $$->b.i36 = $3->value; }
    ;

opterrlist: /* empty */         { $$ = NULL;                                }
    | exprlist '\'' SYMBOL '\'' { $$ = newList(); $$->l = $1; $$->p = $3;   } /* change to exprlist */
    |          '\'' SYMBOL '\'' { $$ = newList(); $$->l = NULL; $$->p = $2; }
    | exprlist                  { $$ = newList(); $$->l = $1; $$->p = NULL; }
    ;

optarg: /* empty */     { $$ = NULL;    }
    |   arg2
    ;

arg2
    : expr            
    | literal           { $$ = exprLiteral($1);                 }
    | ptr_reg '|' expr  { $$ = exprPtrExpr((int)$1, $3);        }
    | VFDLIT    vfdArgs { $$ = exprLiteral(doVFDLiteral($2));   }
    ;

options
    :             option        { $$ = NULL; DL_APPEND($$, $1);      }
    | options ',' option        { $$ = $1;   DL_APPEND($1, $3);      }
    ;

option
    : SYMBOL                            { $$ = newTuple(); $$->a.p = $1;                                    }
    | SYMBOL '(' INTEGER ')'            { $$ = newTuple(); $$->a.p = $1; $$->b.i = (int)$3;                 }
    | SYMBOL '(' SYMBOL '=' INTEGER ')' { $$ = newTuple(); $$->a.p = $1; $$->b.p = $3; $$->c.i = (int)$5;   }
    | SYMBOL '(' SYMBOL '=' SYMBOL  ')' { $$ = newTuple(); $$->a.p = $1; $$->b.p = $3; $$->c.p = $5;        }
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
    static int yyErrCount = 0;
    
    if (FILEsp == 0)
        fprintf(stderr, "Error on line %d: %s near token '%s'\n", yylineno, msg, yytext);
    else
        fprintf(stderr, "Error on line %d in %s: %s near token '%s'\n", yylineno, LEXCurrentFilename(),msg, yytext);
    yyErrorCount += 1;
    fprintf(stderr, "%s\n", LEXline);
    
    yyErrCount += 1;
    
    if (yyErrCount > 10)
    {
        fprintf(stderr, "Too many errors. Aborting.\n");
        exit(1);
    }
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


//word36 getValue(char *s)
//{
//    symtab *y = getsym(s);
//
//    if (!y)
//    {
//        if (nPass == 1)
//            return -1;
//        
//        yyprintf("undefined symbol <%s>", s);
//        return 0;
//    }
//    
//    return y->value;
//}



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


