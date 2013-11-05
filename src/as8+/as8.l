%{
    #include <ctype.h>
    #include <math.h>
    
    #include "as.h"
    
    #include "y.tab.h"

    extern YYSTYPE yylval;

    int litCount = 0;

    int litPosInLine = 0;  // literal position/number in current line

    char *ltrim(char *s);
    char *rtrim(char *s);
    char *trim(char *s);
    
    word36  scaleInt(char *);
    word72  scaleReal(char *);  // XXX change to 72-bit returns

    //eNumericMode nMode = eNumDefault;
    
    bool oMode = false;     // when true only looking for octal numbers
    bool dMode = false;     // when true only looking for decimal numbers
    bool bTermCond = false; // when true were looking for a rptx terminal condition
    bool bvd = false;
    
    bool bReg = false;      // when true symbol should first be cheked for a register value
    
    bool bOperandOnly = false; // when true we don't want any opcode/pseudoops returned as symbols 
    
    char temp[256];         // for temtpoary storage of stuff
    
    char vfdType = 0;       // what type of vfd field are we workig with?
    int vfdSize = 0;        // vfd field size
    
    symtab *lastLabel = NULL;    // symbol table entry to last label defined
    
    // what type of symbol are we looking for?
    enum looking4 {
        operation,      // opcode or pseudoop
        value,          // somethong from the symbol table?
    } sMode = value;
    
    /*
     * let's rewrite the LEX I/O routines to work in our environment ...
     */
    
    #define YY_INPUT(buf,result,max_size) \
    { \
        int c = LEXgetcs(); \
        result = (c == EOF) ? YY_NULL : (buf[0] = c, 1); \
    }

%}

%option stack

%s IFN

%x expect InString
%x InLiteral InLiteral1 InLiteral2 InLiteral3
%x Junk
%x InVFD InVFD2 expOct
%x OPT

%option noyywrap
/*%option yylineno*/

/*%option debug*/

alpha   		[A-Za-z]
digit   		[0-9]
digit19   		[1-9]
octaldigit      [0-7]
hexdigit        ({digit}|[ABCDEFabcdef])

symbol    		({alpha}|[_.])({alpha}|{digit}|[_.])*

special         [-+,*/%&|^()$<>~:;'\[\]]

include			[Ii][Nn][Cc][Ll][Uu][Dd][Ee]
options         [Oo][Pp][Tt][Ii][Oo][Nn]([Ss]?)

octal           ({octaldigit}{1,12})

decimal         {digit19}{digit}*
decimalscaled   [+-]?({digit}+)[Bb]({digit}+)

hex             0[Xx]{hexdigit}{1,9}

nl				(\r?\n)|[;]
ws      		[ \t\f]+
comment			\".*$

real1    		[-+]?{digit}+\.([dEeE][-+]?{digit}+)?
real2    		[-+]?{digit}+\.{digit}+([dDeE][-+]?{digit}+)?
real3    		[-+]?\.{digit}+([dDeE][-+]?{digit}+)?
real4    		[-+]?{digit}+[dDeE][-+]?{digit}+

real	  		{real1}|{real2}|{real3}|{real4}
realscaled 		{real}[Bb][+-]?{digit}+

GEBCDlit        \=([Hh]([^\n]{1,6}))
ASCIIlit        \=([Aa]([^\n]{1,4}))
AHLiteral       \={digit}{1,2}[aAhH]

ah              [AaHh]

AlphaLiteral    {ASCIIlit}|{GEBCDlit}|{AHLiteral}

OooAhh          [OoAaHh]

OctLiteral      \=[oO0]([-+]?)({octaldigit}{1,12})
DecLiteral      \=[-+]?({digit}{1,12})
SclLiteral      \={decimalscaled}
RscLiteral      \={realscaled}
FltLiteral      \={real}
VFDLiteral      \=[Vv]

ITSLiteral      \=[Ii][Tt][Ss]
ITPLiteral      \=[Ii][Tt][Pp]

everythingelse	.

%%
{options}                         { BEGIN OPT; return OPTIONS;                  }
<OPT>{symbol}                     { yylval.s = strdup(yytext);  return SYMBOL;  }
<OPT>{special}                    { return yytext[0];                           }
<OPT>{decimal}|{octal}|{hex}      { yylval.i = strtoll(yytext, NULL, 0); return INTEGER; }

{include}           { BEGIN IFN;		/* don't move! */	}
<IFN>[^ \f\t\n]+	{ 
                        BEGIN INITIAL;
                        LEXgetPush(trim(yytext));	
                    }
{decimal}           { if (oMode && strcmp(yytext, "0")) REJECT; yylval.i = strtoll(yytext, NULL, 10); return DECIMAL;}
{octal}             { if (dMode && strcmp(yytext, "0")) REJECT; yylval.i = strtoll(yytext, NULL,  8); return OCTAL;  }
{hex}               {                                           yylval.i = strtoll(yytext, NULL, 16); return HEX;    }
{decimalscaled}     { yylval.i = scaleInt(yytext); return DECIMAL;     }
{realscaled}        {
                        //yylval.i = scaleReal(yytext); return DECIMAL;
                        if (strpbrk(yytext, "Dd"))  // a double precision integer
                        {
                            yylval.i72 = scaleReal(yytext);
                            return DOUBLEINT;
                        } else {
                            word72 i72 = scaleReal(yytext);
                            yylval.i = (i72 >> 36) & DMASK;
                            return DECIMAL;
                        }
                    }
{real}              {
                        char temp[132];
                        strcpy(temp, yytext);
    
                        char *d = strpbrk(temp, "Dd");
                        if (d)
                            *d = 'e';   // change Dd exponent to 'e' for strtold()
    
                        long double r = strtold(temp, NULL);
                        yylval.r = r;
                        if (d)
                            return DOUBLE;
                        return SINGLE;
}

{special}           {
                        if (yy_start_stack_ptr > 0 && (yy_top_state() == InVFD) && yytext[0] == ',')
                        {
                            yy_pop_state();
                            oMode = false;
                            dMode = false;
                        }
                        return yytext[0];
                    }

<*>{nl}             {
                        oMode = false;
                        dMode = false;
                        bTermCond = false;
                        bReg = false;
                        bOperandOnly = false;
                        
                        // clear state stack if necessary
                        while (yy_start_stack_ptr > 0)
                            yy_pop_state();
                        BEGIN(INITIAL); 
                        return '\n'; 
                    }

<*>{ws}             { /* No action and no return */}

{symbol}:           {   
                        yylval.s = strdup(yytext);
                        *strchr(yylval.s, ':') = '\0';   // remove :
                        
                        // a label.
                        symtab *s = getsym(yylval.s);
                        
                        switch (nPass)
                        {
                            case 1:
                                if (s == NULL)
                                {
                                    s = addsym(yylval.s, addr);
                                    //s->defnLine = yylineno;
                                    //s->defnFile = strdup(LEXCurrentFilename());
                                    
                                    if (debug) printf("Adding label %s = %6o (%06llo) <%s>:%d\n", yylval.s, addr, s->value, s->defnFile, s->defnLine);
                                } else
                                    yyprintf("found duplicate label <%s>", yylval.s);

                                break;
                            case 2:
                                if (s == NULL)
                                    yyprintf("undeclared label <%s> in pass 2", yylval.s);
                                else
                                    if (addr != s->value)
                                        yyprintf("phase error for symbol <%s> 1:%06o 2:%06o", yylval.s, s->value, addr);
                                break;
                        }

                        lastLabel = s;
                        return LABEL;
                    }

{symbol}            {
                      // this can be a symbol, a pseudo-op or an instruction (or a host of other things)
                      // determine what it is and pass back the proper value/TERMINAL to the parser
                      //
                      // XXX presently we can't handle something like:
                      // XXX      fmp     eight,x2	" EAQ = 8*abs_x**2
                      // XXX because it finds a POP. If we make bZOperandOnly == true after we find an opcode then
                      // XXXthe rpt instructions termination codes break. Find a way to fix this someday.
                      // FIXME: this has been fixed, I believe
                      
                      // XXX not certain why I do this here rather than in the search routine(s)
                      char lcyytext[1024];  // lower-case yytext for opcode & pseudoop searches
                      strcpy(lcyytext, yytext);
                      strlower(lcyytext);
    
                      //if (YY_START == INITIAL)
                      if (bOperandOnly == false)
                      {
                        // an opcode?
                        opCode *o = findOpcode(lcyytext);
                        if (o)
                        {
                            memset(&opnd, 0, sizeof(opnd)); // reset opnd values
                            
                            yylval.o = o;
                            if (o->ndes)
                            {
                                oMode = true;           // MW EIS keywords use octal expressions
                                bOperandOnly = true;    // and the keyword bool() is also a pseudoop.....
                            }
                            
                            if (!o->eis)
                                switch (o->opcode)
                                {
                                    case 0600:  // tze
                                    case 0601:  // tnz
                                    case 0602:  // tnc
                                    case 0603:  // trc
                                    case 0604:  // tmi
                                    case 0605:  // tpl
                                    case 0617:  // tov
                                        if (bTermCond)  // a terminating condition for RPTx instructions
                                        {
                                            yylval.s = o->mne;
                                            return TERMCOND;
                                        }
                                        break;
                                }
                            
                            if (o->syntax != OPCODERPT) // if instruction is not a repeat instruction ...
                                bOperandOnly = true;    // ...only look for other user symbols
                            
                            return o->syntax;
                        }
    
                        pseudoOp *po = findPop(lcyytext);
                        if (po)
                        {
                            bOperandOnly = true;    // only look for other user symbols

                            if (po->token == NULLOP)
                            {
                                BEGIN(Junk);
                                return NULLOP;
                            }
                            
                            yylval.p = po;
                            if (po->flags & epStringArgs)
                            {
                                BEGIN(InString);
                                yylval.p = po;
                            } else if (po->flags & epVFD)
                            {
                                BEGIN(InVFD);
                                yylval.p = po;
                                vfdType = 0;
                                vfdSize = 0;
                                bOperandOnly = true;
                            }
                            
                            if (po->flags & epOCT)
                                oMode = true;       // only want octal/logical expression(s)
                            else
                                oMode = false;
                                
                            if (po->flags & epDEC)
                                dMode = true;       // only want decimal expression(s)
                            else
                                dMode = false;
                            
                            if (po->flags & epDESC)
                                bReg = true;       // looking for register names for DESC
                            else
                                bReg = false;
                            
                            if (po->token == CALL)
                            {
                                if (callingConvention == 1)
                                    return CALLH;   // Honeywell calling convention
                                else
                                    return CALLM;   // Multics calling convention
                            }
                            
                            if (po->token)
                                return po->token;
                            
                            return PSEUDOOP;
                        }
                      }
                      if (bReg)
                      {
                          // a register?
                          int r = getmod(yytext);
                          if (r >= 0 && r < 16)
                          {
                            if (debug) fprintf(stderr, "Found bReg <%s> - %d\n", yytext, r);
                            yylval.i = r;
                            return REG;
                          }
                      }

                      yylval.s = strdup(yytext);
                      return SYMBOL;
                    }

<InString>[^\n]+  {
    //if (debug) fprintf(stderr, "InString 1 <%s>\n", yytext);
    
    char *p = yytext;

    while (isspace(*p))
        p += 1;
        
    char buff[256];
    memset(buff, 0, sizeof(buff));
    
    char *q = buff;
    char first = *p++;
    while (*p && *p != first)
        *q++ = *p++;
    
    //if (debug) fprintf(stderr, "InString 2 <%s>\n", buff);

    int sz = (int)(p - yytext) + 1;
    if (sz < 2)
    {
        yyerror("Ill-formed string");
        REJECT; 
    }
    //if (debug) fprintf(stderr, "InString sz = %d\n", sz);

    yyless(sz);    // only consume what we've used
    BEGIN(INITIAL);
    
    yylval.s = strdup(buff);
    return STRING;
}

{AlphaLiteral} {
    // Alphanumeric literals are hard to parse with lex/yacc because of the varying string counts and general string funkyness.
    // ... So, let's have a little tighter integration between the scanner and parser. Kinda' kludgy, but it works well. (Same thing that they do w/ FORTRAN)
    
    // We've found an Alphanumeric literal with a possibly varying character count.
    // ... return various and seperate tokens to the parser rather than playing with it en mass in the scanner
    
    //fprintf(stderr, "Alphanumeric <%s>\n", yytext); litCount = 0;
    
    BEGIN (InLiteral1);
    
    yyless(1);  // push back everything except the =
    
    //char *end_ptr;
    //int N = (int)strtoll(yytext+1, &end_ptr, 10);
    
    //fprintf(stderr, "N=%d\n", N);
    litCount = -1;

    litPosInLine += 1;
    return '=';
}

<InLiteral1>{digit}{1,2} {
    // recognize the digit count of the literal
    char *end_ptr;
    int N = (int)strtoll(yytext, &end_ptr, 10);
    
    //fprintf(stderr, "N=%d\n", N);
    litCount = N;
    
    BEGIN(InLiteral2);
    yylval.i = N;
    return DECIMAL;
}

<InLiteral1>{ah} {
    // our A/H
    //printf("InLiteral1:AH: found <%s>\n", yytext);

    BEGIN(InLiteral3);
    
    yylval.c = tolower(yytext[0]);
    
    switch(yylval.c)
    {
        case 'a':
            litCount = 4;
            break;
        case 'h':
            litCount = 6;
            break;
    }
    //fprintf(stderr, "N=%d\n", litCount);

    return AH;
}

<InLiteral2>{ah} {
    // our A/H
    //printf("InLiteral2:AH: found <%s>\n", yytext);
    
    BEGIN(InLiteral3);
    
    yylval.c = tolower(yytext[0]);
    return AH;
}

<InLiteral3>[^\n]+ {
    // yytext contains the text of the literal...
    // ... but process only litCount chars and return the rest
    
    char buff[256];
    memset(buff, 0, sizeof(buff));
    strncpy(buff, yytext, litCount);
    
    //fprintf(stderr, "Litstr <%s>\n", buff);
    yyless(litCount);   // return everything we aren't interested in
    
    BEGIN(INITIAL);
    
    yylval.s = strdup(yytext);
    return STRING;
}

{OctLiteral} {
    word36s num = strtoll(yytext + 2, NULL, 8);   // convert octal number to long long
    if (num < 0)
    {
        num = -num;
        num ^= 0400000000000LL; // flip the sign bit
    }
    litPosInLine += 1;
    yylval.i = num;
    return OCTLIT;
}
 
{DecLiteral} {
    word36s num = strtoll(yytext + 1, NULL, 10);   // convert octal number to long long
    litPosInLine += 1;
    yylval.i = num;
    return DECLIT;
}

{SclLiteral} {
    litPosInLine += 1;
    yylval.i = scaleInt(yytext + 1);
    return DECLIT;
}

{RscLiteral} {
    litPosInLine += 1;
    //yylval.i = scaleInt(yytext + 1);
    //return DECLIT;
    if (strpbrk(yytext, "Dd"))  // a double precision integer
    {
        yylval.i72 = scaleReal(yytext + 1);
        return DECLIT2;
    } else {
        word72 i72 = scaleReal(yytext + 1);
        yylval.i = (i72 >> 36) & DMASK;
        return DECLIT;
    }

}

{FltLiteral} {
    char temp[132];
    strcpy(temp, yytext+1);
    
    char *d = strpbrk(temp, "Dd");
    if (d)
        *d = 'e';   // change Dd exponent to 'e' for strtold()
    
    litPosInLine += 1;
    long double r = strtold(temp, NULL);
    yylval.r = r;
    if (d)
        return DBLLIT;
    return SGLLIT;
    
}

{ITSLiteral}    { litPosInLine += 1; return ITSLIT;    }
{ITPLiteral}    { litPosInLine += 1; return ITPLIT;    }

<InVFD>{OooAhh} {
    //printf("Found T-%c\n", yytext[0]);
    
    yylval.c = tolower(yytext[0]);
    vfdType = yylval.c;
    
    oMode = false;
    switch (yylval.c)
    {
        case 'a': return Ta;
        case 'h': return Th;
        case 'o': 
            oMode = true;
            return To;
        default:
        {
            char msg[256];
            sprintf(msg, "InVfd(): unknown vfd type <%c>\n", yylval.c);
            yyerror(msg);
        }
    }
}
<InVFD>{digit}+ {
    yylval.i = atoi(yytext);
    vfdSize = (int)yylval.i;
    
    //printf("Found L-%d\n", yylval.i);
    return L;
}
<InVFD>[/] {
    //printf("Found special <%c>\n", yytext[0]);
    
    if (vfdType == 'a' || vfdType == 'h')
    {
        BEGIN(InVFD2);
        return yytext[0];
    }
    
    vfdType = '?';
    yy_push_state(INITIAL);
    return yytext[0];
}

<InVFD2>[^\n]+ {
    //printf("Found VFD a/h '%c' %d <%s>\n", vfdType, vfdSize, yytext);
    
    int nChars = 0;    // how many characters we want to keep.....
    switch (vfdType)
    {
        case 'a':
            nChars = vfdSize / 9;
            nChars += vfdSize % 9 ? 1 : 0;
            break;
        case 'h':
            // 6-bit BCD packed into 9-bits
            //nChars = vfdSize / 6;
            //nChars += vfdSize % 6 ? 1 : 0;
            nChars = vfdSize / 9;
            nChars += vfdSize % 9 ? 1 : 0;
        
            break;
    }
    vfdType = '?';
    //vfdSize = 0;
    
    memset(temp, 0, sizeof(temp));  // clear temp array
    strncpy(temp, yytext, nChars);
    
    yyless((int)strlen(temp));       // push back what we didn't use

    //printf("  nChars = %d %d <%s>\n", nChars, (int)strlen(temp), temp);

    BEGIN(InVFD);
    yy_push_state(INITIAL);

    yylval.s = strdup(temp);
    return STRING;
}

<InVFD>[(] {
    return yytext[0];
}

{VFDLiteral} {
    vfdType = 'v';
    bOperandOnly = true;
    litPosInLine += 1;

    BEGIN(InVFD);
    return VFDLIT;
}

^.+ { litPosInLine = 0; REJECT; }

^{ws}*\*[^\n]*  { BEGIN (Junk); }

<Junk>[^\n]+     {}  /* try to ignore everything else on line */
<*>{comment}     {}  /* keep at the end */
{everythingelse} {}

<<EOF>>     { if (debug) printf("yylex(EOF):addr=%06o\n", addr); return EOF; }

%%

/// Fixed-Point
/// A fixed-point number possesses the same characteristics as the floating-point number with one exception: it must have a third part present. This is the binary scale factor denoted by the letter B followed by a signed or unsigned integer. The binary point is initially assumed at the left-hand part of the word between bit positions 0 and 1. It is then adjusted by the specified binary scale factor with plus implying a shift to the right and with minus, a shift to the left. Double-precision fixed-point follows the rules of double-precision floating-point with the addition of the binary scale factor. AF52-1 GMAP Course hase some good examples .....
// A binary scale factor beginning with "b" indicates fixed point and forces conversion from floating point. The binary point in a literal with a binary scale factor is positioned to the right of the bit indicated by a decimal integer following the "b". (AK92-2. pg.6-12)

//        wamontgomery@ieee.org (http://home.comcast.net/~wamontgomery)
//
//         Most of my assembly experience with the Honeywell/GE line was at Dartmouth, where we used a different assembler with different literal formats, but I think this is the same notation PL/1 used, which I always found a bit confusing.  The way I thought of it was that NbM specifies N with 35-M zero bits to the right.  (If N isn't an integer but has a decimal point, 35-M is where the integer part starts, and the conversion of whatever is after the decimal point begins there.)  What makes it a bit tricky in these instructions is that du and dl modification take 18 bit values, and apparently alm uses the lower half of the value specified (which is why "M" is bigger than 17 in all these examples, otherwise the whole bottom half would be zeros.
//
//         So, for example, 63b23 has 35-23 or 12 zero bits (four 0 octal digits), and then 63 converts to 77 in octal.
//         7b25 has 10 zero bits, or 3 octal digits plus one more.  That means the "7" will start one bit left.  Shift 7 in octal left and you get 16.  18b25 converts similarly (18 in octal is 22, shift left 1 and you get 44
//
//         Etc.
//
//         Like I said, it gets more complicated if you specify fractional values this way
        
        
//
//         Hi Harry,
//         Well based upon the octal data generated to the right of the instruction codes, I think it works as follows.
//
//         ·         Consider the instruction operand as holding a bin(35) value: a signed, binary value stored in 36 bits.
//         o   Bits are numbered from left, with bit position 0 holding the sign in our bin(35) value.
//         o   So =1b35  is a constant placing a decimal 1 in the right-most bit of our bin(35) value:   000000000001 in octal
//         §  But instructions only have room for an 18-bit unsigned operand, so only the right-most 6 octal digits are used:  000001
//         §  That means the scale factor will range from:  19 to 35
//         ·         So =1b32 is:  000010 (in octal); and that is what is show in the octal data generated by the instruction
//         ·         =7b25 puts an decimal 7 followed by 10 binary digit positions of 0; in binary:  000 001 110 000 000 000  = 016000 in octal
//         ·         =2b25 puts an decimal 2 followed by 10 binary digit positions of 0; in binary:  000 000 100 000 000 000  = 004000 in octal
//         ·         =18b25 puts a decimal 18 (octal: 22) followed by 10 binary 0s:                  000 100 100 000 000 000  = 044000 in octal
//         ·         =63b23 puts a decimal 63 (octal: 77) followed by 12 binary 0s:                  111 111 000 000 000 000  = 770000 in octal
//
//
//         Gary
//         gary_dixon@q.com
//

word36 scaleInt(char *si)
{
    char s[256];
    strcpy(s, si);
    char *b = strpbrk(s, "Bb");
    if (*b)
    {
        *b = ' ';

        word36 size;
        int cnt;
        sscanf(s, "%lld %d", &size, &cnt);
        
        // of the form MbN
        word36 mVal = size;
        int nVal = cnt;
        
        if (nVal < 0 || nVal > 35)
            goto bad;

        // right number of bits = 35-N
        word36 MbN = mVal << (35-nVal);
        return MbN & DMASK;
    } else
bad:    yyprintf("scaleInt(%s): Error in evaluating scaled literal", si);
    
    return 0;
}
    
        /*
          of the format MbN
         
         lda     =5b17               " 000005 000000
         
         
         lda     =22.5b5             " 264000 000000
 (ok)    22.5 = 012550000000
                01255       s
                000 001 01  0 101 101
                   E=5        10 110 1--   (34-5 => 29*'0' 36-len of mantissa)
                010110.10000000
         
         lda     =1.2e1b32           " 000000 000140
         
(ok)     000 000 000 000 000 000 000 000 000 001 100 . 000 (DP to the R of bit 32)

         12. =  010600000000
         12. =  0106
                000 001 00 | 0 | 11
                   E=4     | 0 | 110 000 0     (34-32 => 2*0's = 1
         
         lda     =1.95d1b37          " 000000 000004
                                     " 700000 000000
         
         dec     1.95d1     012470000000 000000000000
         
                            012470000000 000000000000
                            000 001 01 | 0 100 111 000 ...
                                E=5
         
         0                                             3 3                                             7
                                                       5 6                                             1
         000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000
(ok)                                                 100 11.1 (DP to the R of Bit 37)
         
         
         
         mpy     =.3012056b-1        " 232157 173225
                                     " 010 011 010 001 101 111 001 111 011 010 010 101
         " 776464336366     dec     .3012056
         111 111 110 100 110 100 011 011 110 011 110 110
         111 111 11 0 | 100 110 100 011 011 110 011 110 110
                    0   100 110 100 011 011 110 011 110 110 ... 10 010 101 (232157 173225)
         
         
         mpy     =.301205b-1         " 232157 052606
                                     " 010 011 010 001 101 111 000 101 010 110 000 110
         
         " 776464336125     dec     .301205
         111 111 110 100 110 100 011 011 110 001 010 101
         111 111 11 | 0 100 110 100 011 011 110 001 010 101
                      0 100 110 100 011 011 110 001 010 101 ... 100 00 110 (232157 052606)
         
         with plus implying a shift to the right and with minus, a shift to the left.
         
         The scale factor is represented by the letter B, followed by an optional sign and one or two digits. A plus sign or no sign indicates that the digit(s) represents the bit position of the first bit in the fractional part; a minus sign indicates that the fractional part initiates the specified number of bits to the left of the word.
         
         5. Fixed-Point
         A fixed-point number possesses the same characteristics as the floating-point number with one exception: it must have a third part present. This is the binary scale factor denoted by the letter B followed by a signed or unsigned integer. The binary point is initially assumed at the left-hand part of the word between bit positions 0 and 1. It is then adjusted by the specified binary scale factor with plus implying a shift to the right and with minus, a shift to the left. Double-precision fixed-point follows the rules of double-precision floating-point with the addition of the binary scale factor.
         
         */
    
word72 scaleReal(char *s)
{
    char raw[256];
    strcpy(raw, s);
    
    *strpbrk(raw, "Bb") = ' ';
    
    bool bDouble = false;
    char *d = strpbrk(raw, "dD");
    if (d)
    {
        bDouble = true;
        *d = 'e';
    }
    
    long double r;
    int n;
    
    sscanf(raw, "%Lf %d", &r, &n);
    
    long double integ = 0;
    long double frac = fabsl(modfl(r, &integ));
    
    word72s i72 = integ;
    
    word72 res72 = 0;
    {
        word72 i = (word72)1 << 71;
        while (i)
        {
            if (i72 >= i)
            {
                res72 |= i;   
                i72 -= i;
            }
            i /= 2;
        }
    }
    
    word72 res72f = 0;
    {
        long double bitval = 0.5;    ///< start at 1/2 and go down .....
        for(int n = 71 ; n >= 0 && r > 0; n -= 1)
        {
            if (frac >= bitval)
            {
                frac -= bitval;
                
                res72f |= ((word72)1 << n);
            }
            bitval /= 2.0;
        }
    }
    
    
    // we now want to shift the binary point R/L according to the specified number of positions
    // <0 => shift to the Left
    
    int b = n;  // -1
    
    
    word72 b72 = 0;
    
    if (b < 0)
    {
        // if b < 0 then only copy fraction
        b = -b;
        b72  = bitfieldInsert72(b72, res72f, b-1, 72);
    } else if (b == 0)
    {
        if (res72 & 1)  // if b == 0 then we are only interested in the 1st bit of the integer
            b72 |= ((word72)1 << 71);
        b72  = bitfieldInsert72(b72, res72f, 0, 71);
    } else if (b > 0)
    {
        word72 i = bitfieldExtract72(res72, 0, b+1);        // integral b+1 bits
        b72 = bitfieldInsert72(b72, i, 72-(b+1), 72-b);
        if (res72f)
        {
            word72 f = res72f >> (b+1);     // fractional bits
            
            b72 |= f;
        }
        
    }
    
//    static word36 yPair[2];
//    
//    yPair[0] = (b72 >> 36) & DMASK;
//    yPair[1] = b72 & DMASK;
//    
//    return yPair;
    
    //return (b72 >> 36) & DMASK;
    return b72;
}

/*
 * Custom I/O for flex ...
 */

/*
 * Stuff for include file support ...
 */
#define MAXINCL	64					///< maximum include nesting
#define FILEtos	FILEstack[FILEsp]	///< we have a stack of FILE*'s

IOstack FILEstack[MAXINCL];
int  FILEsp = -1;		/*!< (FILE*) stack pointer		*/

extern char *srcFile;

char *LEXp = 0;
char LEXline[2560];  // current input line

char *
LEXCurrentFilename()
{
	if (FILEsp < 0)
    return "<No Current File>";
    
	return FILEtos.fname;
}

int
LEXgetc(void)
{
	static int first_call = true;
    
	char c;
    
	if (first_call) {		// on 1st call do special processing
		first_call = false;	/* no longer the 1st call	*/
		FILEsp = -1;			// reset file stack pointer
		LEXgetPush(inFile);	// push configuration file onto stack
        
        if (debug)
            fprintf(stderr, "Reading source code from %s...\n", inFile);
	}
    
	do {
		// read file on top of stack (if possible)
		if (FILEsp < 0)
            c = EOF;					// nothing left to read
		else
            c = fgetc(FILEtos.fp);		/* get character from TOS	*/
        
		if (c == EOF) { /* no more left. Close file	*/
			if (LEXgetPop() == -1)
            {
                first_call = true;  // reset
				return EOF;
			}
            else
                continue;
		} else
        return c;
	} while (1);
}

int
LEXgetcs(void)
{
	static int first_call = true;
    
	char c;
    
	if (first_call) {		// on 1st call do special processing
		first_call = false;	/* no longer the 1st call	*/
		FILEsp = -1;		// reset file stack pointer
		LEXgetPush(inFile);	// push configuration file onto stack
        LEXp = NULL;
        
        if (debug)
            fprintf(stderr, "Reading source code from %s...\n", inFile);
	}
    
	do {
		// read file on top of stack (if possible)
		if (FILEsp < 0)
            return EOF;			// nothing left to read
		else
        {
            if (!LEXp || *LEXp == '\0') // end of buffer ?
            {
                LEXp = fgets(LEXline, sizeof(LEXline), FILEtos.fp);		/* get line from TOS	*/
                if (LEXp)
                {
                    yylineno += 1;
                    if (!strchr(LEXline, '\n'))
                        strcat(LEXline, "\n");
                }
                //  if (nPass == 1 && debug && LEXp)
                //      fprintf(stderr, "LEXfgetcs(%d): %s", yylineno, LEXline);
            }
            
            if (LEXp == NULL) { /* no more left. Close file	*/
                if (LEXgetPop() == -1)
                {
                    first_call = true;  // reset
                    return EOF;
                }
                else
                    continue;
            }
            c = *LEXp++;   // get next char from buffer
            
            return c;
        }
	} while (1);
}

char *
LEXfgets(char *line, int nChars)
{
    /// XXX modify LEXfgets to generate lineno/include specs (e.g. 1-1, 52-153, 1, 4) ala alm) 6-digits wide
	static int first_call = true;
    
	if (first_call) {		// on 1st call do special processing
		first_call = false;	/* no longer the 1st call	*/
		FILEsp = -1;			// reset file stack pointer
		LEXgetPush(inFile);	// push configuration file onto stack
        
        if (debug)
            fprintf(stderr, "Reading source code from %s...\n", inFile);
	}
    
	do {
        char *c = 0;
		// read file on top of stack (if possible)
		if (FILEsp < 0)
            c = NULL;					// nothing left to read
		else
        {
			c = fgets(line, nChars, FILEtos.fp);		/* get line from TOS	*/
            if (debug || c)
                fprintf(stderr, "LEXfgets() %s...\n", c);
        }
		if (c == NULL) { /* no more left. Close file	*/
			if (LEXgetPop() == -1)
            {
                first_call = true;  // reset
                LEXp = 0;
				return NULL;
			}
			else
                continue;
		} else
        {
            yylineno += 1;
            return c;
        }
	} while (1);
}

int
LEXgetPop(void)
{
	if (FILEsp < 0)
        return -1;
    
	if (FILEtos.fp) {			/* close file if not NULL	*/
		fclose(FILEtos.fp);
		FILEtos.fp = NULL;
	}
    
	free (FILEtos.fname);	// no longer needed
	FILEtos.fname = NULL;
    
	FILEsp--;
	yylineno = FILEtos.lineno;	// restore lineno
    
    LEXp = 0;       // reset buffer pointer
    
	return FILEsp;
}

void
LEXfseekEOF()
{
    fseek(FILEtos.fp, 0, SEEK_END);
}

/**
 * try to open an include file...
 */
void
LEXgetPush(char *fileToOpen)
{
    char filename[1024];
    strcpy(filename, fileToOpen);
    
	FILE *isrc;
    
	if (FILEsp >= MAXINCL - 1) {
		fprintf(stderr, "ERROR: Too many nested include's! Sorry.\n");
		return;
	}
    
	if (filename == NULL) {
		fprintf(stderr, "LEXgetPush(\"%s\"): filename == NULL!\n", filename);
		return;
	}
    
	if (filename[0] == '"')	// && filename[strlen(filename)-1] == '"')
        stripquotes(filename);
    
    // try to open file as specified, dir where src file is and along include path ...
    
	isrc = fopen(filename, "r");		/* try to open requested file	*/
    
    if (isrc)
        goto onOpen;
    
    /// see if srcFile has a path associated with it. If so, try it ...
    char *p = strrchr(srcFile, '/');
    if (p)
    {
        // extract directory component of src file and try to open file there ...
        memset(filename, 0, sizeof(filename));
        int n = (int)(p - srcFile) + 1;
        strncpy(filename, srcFile, n);
        
        strcat(filename, fileToOpen);
        
        if (debug) printf("Trying ... <%s>\n", filename);
        
        isrc = fopen(filename, "r");		/* try to open requested file	*/
        
        if (isrc)
            goto onOpen;
    }
    
    char *path;
    char *ip = strdup(includePath);
    while ((path = Strsep(&ip, ";")) != NULL)
    {
        if (strlen(strchop(path)) == 0)    // empty path, just ignore
            continue;
        
        strcpy(filename, path);
        strcat(filename, "/");  // start with path name
        strcat(filename, fileToOpen);
        trim(filename);      // remove any leading/trailing whitespace
        
        if (debug)
            fprintf(stderr, "LEXgetPush(): Trying ... <%s>\n", filename);
        
        isrc = fopen(filename, "r");
        if (isrc)
            break;  // success
    }
    if (isrc == NULL)
    {
		fprintf(stderr, "WARNING: Unable to open file \"%s\". Sorry!\n", filename);
		exit(5);
	}
    
    onOpen:;
	/** .include file is open, so set up to use it ... */
	FILEtos.lineno = yylineno;			// save lineno
	FILEstack[++FILEsp].fp = isrc;		/* make it TOS	*/
//	FILEtos.lineno = yylineno;			// save lineno
	FILEtos.fname = strdup(filename);	// save filename
	yylineno = 0;                       // reset lineno for new file
    LEXp = 0;
    
    if (verbose)
        fprintf(stderr, "Reading from %sfile <%s>\n", FILEsp == 0 ? "" : "include ", filename);
}

/*
 * pop lex stack and reset to initial state ... 
 * (usefull to get out of VFD mode)
 */
void popAndReset()
{
    // clear state stack if necessary
    while (yy_start_stack_ptr > 0)
        yy_pop_state();
    BEGIN(INITIAL);

}

void setOmode()
{
    dMode = false;
    oMode = true;
}
