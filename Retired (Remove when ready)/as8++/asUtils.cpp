/**
 * \file asUtils.c
 * \project as8
 * \author Harry Reed
 * \date 10/6/12
 *  Created by Harry Reed on 10/6/12.
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "as.h"
#include <math.h>

using namespace std;

extern int yylineno;
extern char* yytext;

/*
 * simple symbol table management stuff ...
 */

//symtab *Symtab = NULL;

mapSymtab *SymbolTable = new mapSymtab();

/**
 * initialize symbol table ....
 */
void initSymtab()
{
    //Symtab = NULL;
    
    SymbolTable->clear();
}

PRIVATE symtab *newSym()
{
    return new symtab();    // (symtab*)calloc(1, sizeof(symtab));
}

PRIVATE symref *newSymxref()
{
    return (symref*)calloc(1, sizeof(symref));
}

/*
 * see if symbol already exists ...
 */
symtab* getsym(char const *sym)
{
    if (sym == 0)
    {
        yyerror("getsym(): sym == 0");
        return NULL;
    }
    
    mapSymtab::iterator it = SymbolTable->find(sym);
    if (it == SymbolTable->end())
        return NULL;
    
    return it->second;
#if OLD_WAY
    symtab *s;
    HASH_FIND_STR(Symtab, sym, s );  /* s: output pointer */
    
    // test to see if map table has the symbol.
    if (s)
    {
        mapSymtab::iterator it = SymbolTable->find(sym);
        if (it == SymbolTable->end())
            fprintf(stderr, "Cannot find mapped symbol <%s>\n", sym);
    }
    
    return s;
#endif
}

symtab *addsym(char * sym, word36 value) {
    symtab *s = getsym(sym);
    if (s != NULL)
        return NULL;    // symbol already defined

    s = new symtab();   //Sym();
    
    s->name = sym;      // already strdup()'s in yylex() .... strdup(sym);
    s->value = value;
    
    s->segname = NULL;
    s->extname = NULL;
    
    //HASH_ADD_KEYPTR(hh, Symtab, s->name, strlen(s->name), s );
    
    SymbolTable->insert(mapSymtab::value_type(s->name, s));
    
    return s;
}

symtab *addxref(char *sym, int line, char *file)
{
    symtab *s = getsym(sym);
    if (s != NULL)
        return NULL;    // symbol already defined
    
 // XXX not finished  
    return s;
}

int name_sort(symtab *a, symtab *b)
{
    return strcasecmp(a->name,b->name);
}

void dumpSymtab(bool bSort)
{
    printf("======== %sSymbol Table ========\n", bSort ? "Sorted " : "");

    
    
#if OLD_WAY
    if (bSort)
        HASH_SORT(Symtab, name_sort);
    
    int i = 0;
    symtab *s = Symtab;
    while (s)
    //for (Symtab::iterator it=SymbolTable->begin(); it != SymbolTable->end(); ++it)
	{
        //symtab *s = it->second;
        char temp[256];
        
        if (s->segname)
            sprintf(temp, "%s$%s", s->segname, s->name);
        else
            sprintf(temp, "%s", s->name);
        
        printf("%-10s %06llo   ", temp, s->value);
        i++;
        if (i % 4 == 0)
            printf("\n");
        
        s = (symtab*)s->hh.next;
	}
    if (i % 4)
        printf("\n");
#endif

    int lWidth = 0;
    int rWidth = 0;

    for (mapSymtab::iterator it = SymbolTable->begin(); it != SymbolTable->end(); ++it)
    {
        symtab *s = it->second;

        lWidth = max2(lWidth, (int)strlen(s->name));
        if (s->segname)
            rWidth = max2(rWidth, (int)strlen(s->segname));                
    }
    

    if (bSort)
    {
        for (mapSymtab::iterator it = SymbolTable->begin(); it != SymbolTable->end(); ++it)
        {
            symtab *s = it->second;
            char temp[256];
            
            if (s->segname)
                sprintf(temp, "%*s$%*s", lWidth, s->segname, rWidth, s->name);
            else
                sprintf(temp, "%*s", lWidth, s->name);
            
            printf("%-10s %06llo\n", temp, s->value & AMASK);
        }
    }
    else
    {
        // print symbols in value order ......
        std::multimap<word36, symtab *> vMap;
        
        for (mapSymtab::iterator it = SymbolTable->begin(); it != SymbolTable->end(); ++it)
        {
            symtab *s = it->second;
            
            vMap.insert(pair<word36, symtab*>(s->value, s));
        }
        for (std::multimap<word36, symtab *>::iterator it = vMap.begin(); it != vMap.end(); ++it)
        {
            symtab *s = it->second;
            char temp[256];
            
            if (s->segname)
                sprintf(temp, "%32s$%-32s", s->segname, s->name);
            else
                sprintf(temp, "%32s", s->name);
            
            printf("%-10s %06llo\n", temp, s->value & AMASK);
        }
    }
}





/**
 * external linkage section ...
 * (like symbol table stuff it's rather primitive, but functional)
 */
//void initlinkPool()
//{
//    for(int i = 0; i < sizeof(linkPool) / sizeof(linkPair); i++)
//        linkPool[i].segname = NULL;
//}

// get literal associated with this source address
//linkPair* getlinkPair(char *extname)
//{
//    for(int i = 0; i < sizeof(linkPool) / sizeof(linkPair); i++)
//    {
//        if (linkPool[i].segname == NULL)
//            return NULL;
//        if (strcmp(linkPool[i].extname, extname) == 0)
//            return &linkPool[i];
//    }
//    return NULL;
//}

//linkPair *addlink(char *segname, char *extname)
//{
//    linkPair *l = getlinkPair(extname);
//    if (l != NULL)
//        return NULL;
//    int i = 0;
//    while (linkPool[i].segname != NULL && i < sizeof(literalPool) / sizeof(struct literal))
//        i++;
//    if (i >= sizeof(linkPool) / sizeof(linkPair))
//        return NULL;
//    
//    linkPool[i].segname = strdup(segname);
//    linkPool[i].extname = strdup(extname);
//    linkPool[i].addr = -1;
//    
//    return &linkPool[i];
//}

extern int linkCount;
extern word18 linkAddr;

void dumpextRef()
{
    if (linkCount == 0)
        return;
    
    printf("======== External References ========\n");
    
    int i = 0;
    //symtab *s = Symtab;
    
    //while (s->name)
    for (mapSymtab::iterator it = SymbolTable->begin(); it != SymbolTable->end(); ++it)
	{
        symtab *s = it->second;
        if (s->segname && s->extname)
        {
        
        printf("%-10s %-10s %06llo  ", s->segname, s->extname, s->value);
        
        i++;
        if (i % 2 == 0)
            printf("\n");
        }
        s += 1;
	}
    if (i % 4)
        printf("\n");
}

/*
 * fill-in external segrefs before beginning pass 2
 */
void fillExtRef()
{
    if (!linkCount)
        return;
    
    //symtab *s = Symtab;
    
    if ((addr) % 2)    // linkage (ITS) pairs must be on an even boundary
        addr += 1;
    
    linkAddr = addr;    // offset of linkage section
    
    //while (s->name)
    for (mapSymtab::iterator it = SymbolTable->begin(); it != SymbolTable->end(); ++it)
    {
        symtab *s = it->second;
        
        if (s->segname && s->extname)
        {
            s->value = (s->value + addr) & AMASK;
            addr += 2;
        }
        //s += 1;
    }
}

/**
 * write literap pool to output stream "oct" ...
 */
void writeExtRef(FILE *oct)
{
    word18 maxAddr = 0;

    //symtab *s = Symtab;

    if (linkCount)
    {
        if ((addr) % 2)    // ITS pairs must be on an even boundary
            addr += 1;
    
        if (linkAddr && addr != linkAddr)
            fprintf(stderr, "writeExtRef(): Phase error for linkage section %06o != %06o\n", addr, linkAddr);
    }
    else return;

//    while (s->name)
//    {
//        if (s->segname && s->extname)
//        {
//
//            word18 lnkAddr = (word18)s->value;
//            
//            if (addr != s->value)
//                fprintf(stderr, "writeextRef(): Phase error for %s/%s\n", s->segname, s->extname);
//            
//            int segno = 0;  // filled in by loader
//            int offset = 0; // filled in by loader
//            word36 even = ((word36)segno << 18) | 046;  // unsnapped link
//            word36 odd = (word36)(offset << 18);    // no modifications (yet)| (arg3 ? getmod(arg3) : 0);
//        
//            char desc[256];
//            sprintf(desc, "link %s|%s", s->segname, s->extname);
//        
//            fprintf(oct, "%6.6o xxxx %012llo %s \n", (addr)++, even, desc);
//            fprintf(oct, "%6.6o xxxx %012llo\n", (addr)++, odd);
//
//            maxAddr = max(maxAddr, lnkAddr);
//        }
//        s += 1;
//    }
//    addr = maxAddr;
}

/**
 * return normalized dps8 representation of IEEE double f0 ...
 */
float72 IEEElongdoubleToFloat72(long double f0)
{
    if (f0 == 0)
        return (float72)((float72)0400000000000LL << 36);
    
    bool sign = f0 < 0 ? true : false;
    long double f = fabsl(f0);
    
    int exp;
    long double mant = frexpl(f, &exp);
    
    //fprintf(stderr,"sign=%d f0=%Lf mant=%Lf exp=%d\n", sign, f0, mant, exp);
    
    word72 result = 0;
    
    // now let's examine the mantissa and assign bits as necessary...
    
    if (sign && mant == 0.5)
    {
        result = bitfieldInsert72(result, 1, 63, 1);
        exp -= 1;
        mant -= 0.5;
    }

    long double bitval = 0.5;    ///< start at 1/2 and go down .....
    for(int n = 62 ; n >= 0 && mant > 0; n -= 1)
    {
        if (mant >= bitval)
        {
            result = bitfieldInsert72(result, 1, n, 1);
            mant -= bitval;
            //fprintf(stderr, "Inserting a bit @ %d %012llo %012llo\n", n , (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
        }
        bitval /= 2.0;
    }
    //fprintf(stderr, "n=%d mant=%f\n", n, mant);
    
    //fprintf(stderr, "result=%012llo %012llo\n", (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
    // if f is < 0 then take 2-comp of result ...
    if (sign)
    {
        result = -result & (((word72)1 << 64) - 1);
        //fprintf(stderr, "-result=%012llo %012llo\n", (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
    }
    //! insert exponent ...
    int e = (int)exp;
    result = bitfieldInsert72(result, e & 0377, 64, 8);    ///< & 0777777777777LL;
    
    // XXX TODO test for exp under/overflow ...
    
    return result;
}

void IEEElongdoubleToYPair(long double f0, word36 *Ypair)
{
    float72 r = IEEElongdoubleToFloat72(f0);
    
    Ypair[0] = (r >> 36) & DMASK;
    Ypair[1] = r & DMASK;
}

#if BROKEN
void IEEElongdoubleToYPairOLD(long double f0, word36 *Ypair)
{
    if (f0 == 0)
    {
        Ypair[0] = 0400000000000LL;
        Ypair[1] = 0;
        return;
    }
    
    bool sign = f0 < 0 ? true : false;
    long double f = fabsl(f0);
    
    int exp;
    long double mant = frexpl(f, &exp);
    
    if (exp > 127)
    {
        fprintf(stderr, "WARNING: exponent overflow (%Lg)\n", f0);
        exp = 127;
    }
    else if (exp < -128)
    {
        fprintf(stderr, "WARNING: exponent underflow (%Lg)\n", f0);
        exp = -128;
    }
    
    uint64 result = 0;
    
    /// now let's examine the mantissa and assign bits as necessary...
    
    long double bitval = 0.5;    ///< start at 1/2 and go down .....
    for(int n = 62 ; n >= 0 && mant > 0; n -= 1)
    {
        if (mant >= bitval)
        {
            //result = bitfieldInsert72(result, 1, n, 1);
            result |= ((word36)1 << n);
            
            mant -= bitval;
        }
        bitval /= 2.0;
    }

    // if f is < 0 then take 2-comp of result ...
    if (sign)
        result = -result;

    // insert exponent ...
    Ypair[0] = (exp & 0377) << 28;
    // and mantissa
    Ypair[0] |= (result >> 36) & 01777777777LL;
    Ypair[1] = (result << 8) & 0777777777400LL;
}
#endif


char *strrev(char *str) /// in-place strrev()
{
    char *p1, *p2;
    
    if (! str || ! *str)
        return str;
    for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
    {
        *p1 ^= *p2;
        *p2 ^= *p1;
        *p1 ^= *p2;
    }
    return str;
}

char *
stripquotes(char *s)
{
	char *p;
    
	while ((p = strchr(s, '"')))
		*p = ' ';
	strchop(s);
    
	return s;
}

/**
 * Removes the trailing spaces from a string.
 */
char *rtrim(char *s)
{
	int index;
    
	for (index = (int)strlen(s) - 1; index >= 0 && (s[index] == ' ' || s[index] == '\t'); index--)
    {
		s[index] = '\0';
	}
	return(s);
}
/** ------------------------------------------------------------------------- */
char *ltrim(char *s)
/**
 *	Removes the leading spaces from a string.
 */
{
	char *p;
	if (s == NULL)
        return NULL;
    
	for (p = s; (*p == ' ' || *p == '\t') && *p != '\0'; p++)
        ;
    
	strcpy(s, p);
	return(s);
}

/** ------------------------------------------------------------------------- */

char *trim(char *s)
{
	return ltrim(rtrim(s));
}


/**
 * Removes the trailing spaces from a string.
 */
char *strclip(char *s)
{
    return rtrim(s);
}
/** ------------------------------------------------------------------------- */
char *strpreclip(char *s)
/**
 *	Removes the leading spaces from a string.
 */
{
    return ltrim(s);
}

/** ------------------------------------------------------------------------- */

char *strchop(char *s)
{
	return trim(s);
}

/** ------------------------------------------------------------------------- */

/*  state definitions  */
#define	STAR	0
#define	NOTSTAR	1
#define	RESET	2

int strmask(char *str, char *mask)
/**
 Tests string 'str' against mask string 'mask'
 Returns TRUE if the string matches the mask.
 
 The mask can contain '?' and '*' wild card characters.
 '?' matches any	single character.
 '*' matches any number of any characters.
 
 For example:
 strmask("Hello", "Hello");	---> TRUE
 strmask("Hello", "Jello");	---> FALSE
 strmask("Hello", "H*o");	---> TRUE
 strmask("Hello", "H*g");	---> FALSE
 strmask("Hello", "?ello");	---> TRUE
 strmask("Hello", "H????");	---> TRUE
 strmask("H", "H????");		---> FALSE
 */
{
	char *sp, *mp, *reset_string, *reset_mask, *sn;
	int state;
    
	sp = str;
	mp = mask;
    
	while (1) {
		switch (*mp) {
            case '\0':
                return(*sp ? false : true);
            case '?':
                sp++;
                mp++;
                break;
            default:
                if (*mp == *sp) {
                    sp++;
                    mp++;
                    break;
                } else {
                    return(false);
                }
            case '*':
                if (*(mp + 1) == '\0') {
                    return(true);
                }
                if ((sn = strchr(sp, *(mp + 1))) == NULL) {
                    return(false);
                }
                
                /* save place -- match rest of string */
                /* if fail, reset to here */
                reset_mask = mp;
                reset_string = sn + 1;
                
                mp = mp + 2;
                sp = sn + 1;
                state = NOTSTAR;
                while (state == NOTSTAR) {
                    switch (*mp) {
                        case '\0':
                            if (*sp == '\0')
                                return(false);
                            else
                                state = RESET;
                            break;
                        case '?':
                            sp++;
                            mp++;
                            break;
                        default:
                            if (*mp == *sp) {
                                sp++;
                                mp++;
                            } else
                                state = RESET;
                            break;
                        case '*':
                            state = STAR;
                            break;
                    }
                }
                /* we've reach a new star or should reset to last star */
                if (state == RESET) {
                    sp = reset_string;
                    mp = reset_mask;
                }
                break;
		}
	}
	return(true);
}


/** -------------------------------------------------------------------------- */

char *strwrap(char *text, int *row, int width)
/**
 Takes a string and copies it into a new string.
 Lines are wrapped around if they are longer than width.
 returns a pointer to the space allocated for the output.
 or NULL if out of memory.
 
 'text'	the string to word wrap
 'row'	the number of rows of text in out
 'width'	the width of the text rows
 
 Wrap algorithm:
 Start at beginning of the string.
 Check each character and increment a counter.
 If char is a space remember the location as a valid break point
 If the char is a '\n' end the line and start a new line
 If WIDTH is reached end the line at the last valid break point
 and start a new line.
 If the valid break point is at the beginning of the line
 hyphenate and continue.
 */
{
	char	*string;			/*!< the output string */
	char	*line;				/*!< start of current line */
	char   	*brk; 				/*!< most recent break in the text */
	char   	*t, *s;
	size_t  len;
	bool	done = false;
    
	*row = 0;
    
	/* allocate string space; assume the worst */
	len = strlen(text);
    
	len = (len > 0x7fff) ? 0xffff : len * 2 + 1;	// stupid PC's...
    
    //	if ((string = (char *) malloc(len)) == NULL)
	if ((string = (char*)malloc(len)) == NULL)
		return(NULL);
    
	if (*text == '\0' || width < 2) {
		strcpy(string, text);
		return(string);
	}
    
	*string = '\0';
	line = string;
    
	for (t = text; !done; ) {
		for(brk = s = line; (s - line) < width; t++, s++) {
			*s = *t;
			if (*t == '\n' || *t == '\0') {
				brk = s;
				break;
			} else if (*t == ' ') {
				brk = s;
			}
		}
        
		if (brk == line && *t != '\n' && *t != '\0') {
			/* one long word... */
			s--;
			t--;
			*s = '\n';
			*(s + 1) = '\0';
			line = s + 1;
		} else if (*t == '\n') {
			*s = '\n';
			*(s + 1) = '\0';
			t++;
			if (*t == '\0') {
				done = true;
			} else {
				line = s + 1;
			}
		}
		else if (*t == '\0') {
			*s = '\0';
			done = true;
		} else {
			/* chop off last word */
			t = t - (s - brk) + 1;
			*brk = '\n';
			*(brk + 1) = '\0';
			line = brk + 1;
		}
        
		(*row)++;
	}
    
	return(string);
}
/** ------------------------------------------------------------------------- */

/**
 * strtok() with string quoting...
 * (implemented as a small fsm, kinda...
 * (add support for embedded " later, much later...)
 */
#define NORMAL 		1
#define IN_STRING	2
#define EOB			3

char *
Strtok(char *line, char *sep)
{
    
	static char *p;		/*!< current pointer position in input line	*/
	static int state = NORMAL;
    
	char *q;			/*!< beginning of current field			*/
    
	if (line) {			/* 1st invocatio						*/
		p = line;
		state = NORMAL;
	}
    
	q = p;
	while (state != EOB) {
		switch (state) {
			case NORMAL:
				switch (*p) {
					case 0:				///< at end of buffer
						state = EOB;	// set state to "end Of Buffer
                        return q;
                        
                    case '"':		///< beginning of a quoted string
                        state = IN_STRING;	// we're in a string
                        p++;
                        continue;
                        
                    default:    ///< only a few special characters
                        if (strchr(sep, *p) == NULL) {	// not a sep
                            p++;				// goto next char
                            continue;
                        } else {
                            *p++ = (char)NULL;	/* ... iff >0	*/
                            while (*p && strchr(sep, *p))	/* skip over seperator(s)*/
                                p++;
                            return q;	/* return field		*/
                        }
                }
                
			case IN_STRING:
                if (*p == 0) {		  /*!< incomplete quoted string	*/
                    state = EOB;
                    return q;
                }
                
                if (*p != '"') { // not end of line and still in a string
                    p++;
                    continue;
                }
                state = NORMAL;			/* end of quoted string	*/
                p++;
                
                continue;
                
			case EOB:					/*!< just in case	*/
				state = NORMAL;
				return NULL;
                
			default:
				fprintf(stderr, "(Strtok):unknown state - %d",state);
				state = EOB;
				return NULL;
        }
        
	}
    
	return NULL;		/* no more fields in buffer		*/
    
}


//char *Strdup(char *s, int size)
//{
//	if (s == NULL) {
//		fprintf(stderr, "(Strdup):s == NULL");
//		return 0;
//	}
//    
//	char *d = malloc(size <= 0 ? strlen(s)+1 : size);
//	if (d)
//		strcpy(d, s);
//	else
//		fprintf(stderr, "(Strdup):d == NULL:<%s>", s);
//	return d;
//}

char *
strlower(char *q)
{
	char *s = q;
    
	while (*s) {
		if (isupper(*s))
			*s = tolower(*s);
		s++;
	}
	return q;
}

/**
 * Expand C style escape sequences ...
 * added octal/hex escapes 21 Nov 96, HWR
 * replaced sscanf with strtol 25 Nov 12, HWR
 */
char *
strexp(char *d, char *s)
{
    char *r = d;
	long val;
	char *end_ptr;
    
    do {
h:      switch (*s) {
            case '\\' : /*!< An escape sequence */
                s++;
                switch (*s) {
                    case '0':	///< an octal or hex
                    case '1':	///< a decimal digit
                    case '2':	///< a decimal digit
                    case '3':	///< a decimal digit
                    case '4':	///< a decimal digit
                    case '5':	///< a decimal digit
                    case '6':	///< a decimal digit
                    case '7':	///< a decimal digit
                    case '8':	///< a decimal digit
                    case '9':	///< a decimal digit
                        val = strtoll(s, &end_ptr, 0); // allows for octal, decimal and hex
                        if (end_ptr == s)
                            fprintf(stderr, "strexp(%s): strtoll conversion error", s);
                        else
                            s = end_ptr;
                        *d++ = val & 0xff;
                        goto h;
                    case 'a' :  /*!< A bell       */
                        *d++ = '\a';
                        break;
                    case 'b' :  /*!< Backspace    */
                        *d++ = '\b';
                        break;
                    case 'f' :  /*!< A Form feed  */
                        *d++ = '\f';
                        break;
                    case 'n' :  /*!< a nl <CR><LF> */
                        //		       *d++ = '\r';    
                        *d++ = '\n';
                        break;
                    case 'r' :  /*!< A Carriage return    */
                        *d++ = '\r';
                        break;
                    case 't' : /*!< A tab         */
                        *d++ = '\t';
                        break;
                    case 'v' :
                        *d++ = '\v';
                        break;
                    case '\\' :
                        *d++ = '\\';
                        break;
                    case '"':
                        *d++ = '"';
                        break;
                    case '\'':
                        *d++ = '\'';
                        break;
                }
                break;
            default : *d++ = *s;
        }
    } while (*s++);
    return (r);
}

/** ------------------------------------------------------------------------- */
/** copy a string while predicate function returns non-0                      */

int strcpyWhile(char *dst, char *src, int (*f)(int))
{
    int n = 0;
    *dst = '\0';
    while (*src && f(*src))
    {
        *dst++ = *src++;
        *dst = '\0';
        n++;
    }
    return n;
}


char *Strsep(char **stringp, const char *delim)
{
    char *s = *stringp;
    char *e;
    
    if (!s)
        return NULL;
    
    e = strpbrk(s, delim);
    if (e)
        *e++ = '\0';
    
    *stringp = e;
    return s;
}

/*!
 a - Bitfield to insert bits into.
 b - Bit pattern to insert.
 c - Bit offset number.
 d = Number of bits to insert.
 
 Description
 
 Returns the result of inserting bits B at offset C of length D in the bitfield A.
 */
word72 bitfieldInsert72(word72 a, word72 b, int c, int d)
{
    word72 mask = ~((word72)-1 << d) << c;
    mask = ~mask;
    a &= mask;
    return a | (b << c);
}

/**
 a - Bitfield to insert bits into.
 b - Bit pattern to insert.
 c - Bit offset number.
 d = Number of bits to insert.
 
 Description
 
 Returns the result of inserting bits B at offset C of length D in the bitfield A.
 
 NB: This would've been much easier to use of I changed, 'c', the bit offset' to reflect the dps8s 36bit word!! Oh, well.
 */
word36 bitfieldInsert36(word36 a, word36 b, int c, int d)
{
    word36 mask = ~(0xffffffffffffffffLL << d) << c;
    mask = ~mask;
    a &= mask;
    return (a | (b << c)) & DMASK;
}

word36 bitMask36(int length)
{
    return ~(0xffffffffffffffffLL << length);
}

/*!
 a -  Bitfield to extract bits from.
 b -  Bit offset number. Bit offsets start at 0.
 c - Number of bits to extract.
 
 Description
 
 Returns bits from offset b of length c in the bitfield a.
 */
word36 bitfieldExtract36(word36 a, int b, int c)
{
    word36 mask = ~(0xffffffffffffffffLL  << c);
    //printf("mask=%012llo\n", mask);
    if (b > 0)
        return (a >> b) & mask; // original pseudocode had b-1
    else
        return a & mask;
}

word72 bitfieldExtract72(word72 a, int b, int c)
{
    word72 mask = ~((word72)-1 << c);
    if (b > 0)
        return (a >> b) & mask; // original pseudocode had b-1
    else
        return a & mask;
}

