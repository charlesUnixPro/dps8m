//
//  fnp_utils.c
//  fnp
//
//  Created by Harry Reed on 12/9/14.
//  Copyright (c) 2014 Harry Reed. All rights reserved.
//
//
// Utility routines that have served me well for far too many years ...
//

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "fnp_utils.h"

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

#if 0
char *
stripquotes(char *s)
{
    if (! s || ! *s)
        return s;
/*
    char *p;
    
    while ((p = strchr(s, '"')))
        *p = ' ';
    strchop(s);
    
    return s;
*/
    int nLast = (int)strlen(s) - 1;
    // trim away leading/trailing "'s
    if (s[0] == '"')
        s[0] = ' ';
    if (s[nLast] == '"')
        s[nLast] = ' ';
    return trim(s);
}
#endif

#if 0
/**
 * Removes the trailing spaces from a string.
 */
char *rtrim(char *s)
{
    if (! s)
      return s;
    int index;
    
    //for (index = (int)strlen(s) - 1; index >= 0 && (s[index] == ' ' || s[index] == '\t'); index--)
    for (index = (int)strlen(s) - 1; index >= 0 && isspace(s[index]); index--)
    {
        s[index] = '\0';
    }
    return(s);
}
#endif

#if 0
/** ------------------------------------------------------------------------- */
char *ltrim(char *s)
/**
 *	Removes the leading spaces from a string.
 */
{
    if (! s)
      return s;
    char *p;
    if (s == NULL)
        return NULL;
    
    //for (p = s; (*p == ' ' || *p == '\t') && *p != '\0'; p++)
    for (p = s; isspace(*p) && *p != '\0'; p++)
        ;
    
    //strcpy(s, p);
    memmove(s, p, strlen(p) + 1);
    return(s);
}
#endif

#if 0
/** ------------------------------------------------------------------------- */

char *trim(char *s)
{
    return ltrim(rtrim(s));
}
#endif


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

#if 0
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
 
 see also: http://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html
 for a better implementation
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
#endif


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
    if ((string = malloc(len)) == NULL)
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

#if 0
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
    
    if (line) {			/* 1st invocation						*/
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
                            *p++ = (char)0;	/* ... iff >0	*/
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
#endif


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

#if 0
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
#endif

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
                    *d++ = val & 0x1ff; // unfortunately since we aren't using wide characters we're still stuck at 8-bits
                    
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

/*
 * return a static char* representing the current time as specified by TIMEFORMAT
 */

#define TIMEFORMAT "%Y/%m/%d %T"

char *Now()
{
    static char outstr[200];
    time_t t;
    struct tm *tmp;
    
    t = time(NULL);
    tmp = localtime(&t);
    if (tmp == NULL) {
        perror("localtime");
        exit(EXIT_FAILURE);
    }
    
    if (strftime(outstr, sizeof(outstr), TIMEFORMAT, tmp) == 0) {
        fprintf(stderr, "strftime returned 0");
        exit(EXIT_FAILURE);
    }
    return outstr;
}

#if 0
bool startsWith(const char *str, const char *pre)
{
    size_t lenpre = strlen(pre),
    lenstr = strlen(str);
    return lenstr < lenpre ? false : strncasecmp(pre, str, lenpre) == 0;
}
#endif
