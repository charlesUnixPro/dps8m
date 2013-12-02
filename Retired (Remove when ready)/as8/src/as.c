/**
 * \file as.c
 * \project as8
 * \author Harry Reed
 * \date 1/8/13
 *
 * depreciated by as8++
 */

#include "as.h"

bool debug = false;     ///< output debug info
bool verbose = false;   ///< verbose program output

char *inFile = "";

opCode *getOpcodeEntry(const char *op)    
{
    int n;
    // look for non-EIS stuff ...
    for(n = 0 ; n < 01000 ; ++n)
    {
        struct opCode *o = &NonEISopcodes[n];
        if(o->mne)
            if(!strcasecmp(op, o->mne))
            {
                o->opcode = n;
                return o;
            }
    }
    
    // look for EIS stuff ...
    for(n = 0 ; n < 01000 ; ++n)
    {
        struct opCode *o = &EISopcodes[n];
        if(o->mne)
        {
            if(!strcasecmp(op, o->mne))
            {
                o->opcode = n;
                return o;
            }
        }
    }
    fprintf(stderr, "ERROR! Unknown opcode <%s>\n", op);
    return NULL;
}


word18 getOpcode(const char *op)    /// , const char *arg1, const char *arg2) {
{
    int n;
    // look for non-EIS stuff ...
    for(n = 0 ; n < 01000 ; ++n)
    {
        struct opCode *o = &NonEISopcodes[n];
        if(o->mne)
            if(!strcasecmp(op, o->mne))
            {
                o->opcode = n;
                return (n << 3);
            }
    }
  
    // look for EIS stuff ...
    for(n = 0 ; n < 01000 ; ++n)
    {
        struct opCode *o = &EISopcodes[n];
        if(o->mne)
            if(!strcasecmp(op, o->mne))
            {
                o->opcode = n;
                return (n << 3) | 4;    // to specify an EIS
            }
   }
   fprintf(stderr, "ERROR! Unknown opcode <%s>\n", op);
   return 0;
}

word8 getmod(const char *arg_in) {
  word8 mod;
  //char arg[4]; /*!< ?big enough for now */
  char arg[64]; ///< a little bigger
  int n;
  int c1, c2;
  mod = 0;
  for(c1 = c2 = 0 ; arg_in[c1] && (c2 < sizeof(arg)) ; ++c1) {
    if(arg_in[c1] != 'x') {
      /* filter out x's */
      if (debug) printf("getmod filter: \"%s\"[%d]: %c -> [%d]\n", arg_in, c1, arg_in[c1], c2);
      arg[c2++] = arg_in[c1];
    }
  }
  arg[c2] = '\0';

  if (strcmp(arg, "*") == 0)    // HWR 7 Nov 2012 Allow * for an alias for n*
      strcpy(arg, "n*");
    
  for(n = 0 ; n < 0100 ; ++n)
   // if(mods[n])
   //   if(!strcasecmp(arg, mods[n]))
    if (extMods[n].mod && !strcasecmp(arg, extMods[n].mod))
        mod = n;
  if (debug) printf("MOD %s [%s] = %o\n", arg, arg_in, mod);
  return mod;
}

bool isValidLabelChar(char c)
{
    if (isalnum(c))
        return true;
    
    switch (c)
    {
        case '_':
        case '$':
        case '.':
            return true;
        default:
            return false;
    }
    return false;
}

int isValidOpChar(int c)
{
    if (isalnum(c))
        return 1;
    
    switch (c)
    {
        case '.':
            return 1;
        default:
            return 0;
    }
    return 0;
}

bool parseLine(char *inLine, char **label, char **op, char **arg0, char **argv)
{
    if (debug)  fprintf(stderr, "parseLine(): input: <%s>\n", inLine);
    
    *label = NULL;
    *op = NULL;
    *argv = NULL;
    
    char *p = strdup(inLine),
         *p0 = p;
    
    char buff[1024];
    strcpy(buff, p);   // buff is a copy of inLine to play with
    
    strchop(p); // remove leading/trailing spaces
    
    // first char a comment?
    if (p[0] == '"' || p[0] == '*')
    {
        *label = NULL;
        *arg0 = NULL;
        *argv = NULL;
        return true;
    }
    
    // look for a label.....
    
    // NB: as8 only allows 1 label per source line - for now....
    
    char *l = strchr(p, ':');
    if (l) // a possible label ...
    {
        char lbl[256];
        
        sscanf(p, "%s", lbl);          // first token *should* be the label...
        if (strchr(lbl, ':') == 0)     // this aint a label.....
            *label = NULL;
        else
        {
            // last char of 1st token is a ':', so remove it
            *strchr(lbl, ':') = '\0';
            for(int i = 0 ; i < strlen(lbl)-1 ; i++)
            {
                if (!isalnum(lbl[i]))
                    if (!isValidLabelChar(lbl[i]))
                    {
                        fprintf(stderr, "ERROR: Malformed label <%s>\n", lbl);
                        return false;
                    }
            }
            *label = strdup(lbl);  //Label;
            
           // remove any more leading/trailing spaces ...
            // ... get everything after ':' ...
            p += strlen(lbl) + 1;   // skip :
            
            strchop(p);
            strcpy(buff, p);
        }
        
//        strncpy(buff, p, l - p);    //+ 1);    // copy label into buff...
//        
//        if (debug) printf("parseLine(): Label=<%s>\n", buff);
//        
//        // check for a valid label ....
//        for(int i = 0 ; i < strlen(buff)-1 ; i++)
//        {
//            //if (!isalnum(buff[i]))
//            if (!isValidLabelChar(buff[i]))
//            {
//                fprintf(stderr, "WARNING: Potentially malformed label <%s>\n", buff);
//                return false;
//            }
//        }
//        *label = strdup(buff);  //Label;
//        
//        // remove any more leading/trailing spaces ...
//        strcpy(p, l+1); // ... get everything after ':' ...
//        strpreclip(p);  // ... and remove any interstitial spaces
    } else
        *label = NULL;
    
    // we now have any label that may be present
    //if (debug) fprintf(stderr, "parseLine(): ROL(1):<%s>\n", p);
    
    // now, look for operator/mnemonic/etc.
    int n = strcpyWhile(buff, p, &isValidOpChar);    ///< &isalnum);
    //if (debug) fprintf(stderr, "parseLine(): n=%d str=<%s>\n", n, buff);
    if (n)
    {
        *op = strdup(buff);
        
        // remove any more trailing spaces ...
        strcpy(p, p+n); // ... get everything after 'op' ...
        strpreclip(p);  // ... and remove any interstitial spaces
        
        if (debug) fprintf(stderr, "ROL(2):<%s>\n", p);
        
        // at this point we have nothing but args left .....
        *arg0 = strdup(p);  //Arg0;   // entire arg list ......
        
        char *token;
        int n = 0;
        while ((token = Strsep(&p, ",")) != NULL)
        {
            n += 1;
            
            char *c = strchr(token, '\"');  ///< token generally won't have "'s in them, so remove
            if (c)
                *c = '\0';

            // XXX some tokens like "-h    " need the trailing blanks....
            
            if (strlen(strchop(token)) > 0)
            //if (strlen(strpreclip(token)) > 0)
                *argv++ = strdup(token);
            else
                *argv++ = '\0'; // HWR 5 Nov 2012 - allow for 0-length args
            
            if (debug) fprintf(stderr, "argv(%d): <%s>\n", n, *(argv-1));
            if (c)
                break;
        }
        *argv = 0;  // last one is a null
    }
    
    if (debug) fprintf(stderr, "parseLine(): =====================\n");
    
    free(p0);
    return true;
}

word18 addr = 0;
int32 lineno = 0;       // line# of current source file

word18 linkAddr = 0;    // address of linkage section in this assembly
int linkCount = 0;      // # of segrefs (links) for this assembly

void pass(int nPass, FILE *src, FILE *oct)
{
    char line[256];
    char inLine[256];
 
    lineno = 0;

    addr = 0; /* xxx */
    //lineno = 0;

    while(1) {
        char *label, *op, *arg0, *arg1, *arg2, *arg3, *arg4, *args[32] = {0};
        size_t linelen;
        
        //fgets(line, sizeof(line), src);
        //if(feof(src))
        //    break;
        char *s = LEXfgets(line, sizeof(line));
        if (s == NULL)
            break;
        
        lineno += 1;
        
        char *nl = strchr(s, '\n');
        if (nl)
            *nl = '\0';
        
        if (debug) fprintf(stderr, "input line: <%s>\n", line);
        
        memset(args, 0, sizeof(args) / sizeof(args[0]));    // remove any stale arg pointers
        parseLine(line, &label, &op, &arg0, &args[0]);
                
        arg1 = args[0];
        arg2 = args[1];
        arg3 = args[2];
        arg4 = args[3];
        // more if we need 'em ...
         
        if (debug) fprintf(stderr, "label:<%s> op:<%s> arg1:<%s> arg2:<%s> arg3:<%s> arg4:<%s>\n", label, op, arg1, arg2, arg3, arg4);
                
        if (label) {
            if (debug) printf("  LABEL=%s\n", label);

            symtab *s = getsym(label);
            
            if (nPass == 1)
            {
                if (s == NULL)
                {
                    s = addsym(label, addr);
                    if (debug) printf("adding label %s = %6o (%06o)\n", label, addr, s->value);
                } else
                    fprintf(stderr, "found duplicate label <%s>\n", label);
            } else { // pass 2
                if (s == NULL)
                    fprintf(stderr, "undeclared label <%s> in pass 2!!\n", label);
                else
                    if (addr != s->value)
                        fprintf(stderr, "Phase error for symbol <%s> 1:%06o 2:%06o\n", label, s->value, addr);
            }
        }
        
        if (!op)
            continue;
        
        linelen = strlen(line);
        if(line[linelen - 1] == '\n')
            line[linelen - 1] = '\0'; /* strip newline */
        
        if (debug) printf("LINE(#%u)=%s\n", lineno, line);
        
        strcpy(inLine, line);  // save input line
        
        if (doPseudoOp(line, label, op, arg0, &args[0], oct, nPass, &addr))
            goto free;
        
        // an instruction
        
        opCode *o = getOpcodeEntry(op);
        
        if (!o)
        {
            fprintf(stderr, "ERROR: unknown opcode %s @ line %d\n", op, lineno);
            goto free;
        }
        
        // check for multi-word EIS .....
        if (o && o->ndes > 0)
        {
            if (oct)
            {
                word36 EIScode = parseMwEIS(o, arg0);   ///< parse multi-word EIS
                
                fprintf(oct, "%6.6o xxxx %012llo %s\n", addr++, EIScode, inLine);
            }
            else
                addr++;

            goto free;
        }
        
        // XXX look at DH02-01 ... pg 5-26 for floatable code options/ideas
        
        word18 arg = 0;
        word8 mod;
        bool bit29 = false; ///< when true, set bit-29 in instruction
        
        if (arg1)
        {
            // XXX literals should probably be moved into ExprEval() so anybody can use them
            if (arg1[0] == '=')
            {
                if (arg2)
                {
                    if (!(o->flags & NO_TAG))   // mod is *not* a tag?
                    {
                        mod = arg2 ? getmod(arg2) : 0;
                        //fprintf(stderr, "=mod=%o (%s)\n", mod, arg1);
                        
                        if (mod == TD_DU || mod == TD_DL)
                        {
                            /*
                             If a literal is used with the modifier variations DU (direct upper) or DL (direct lower), the value of the literal is not stored in the literal pool but is truncated to an 18-bit value, and is stored in the address field of the machine instruction. Normally, a literal represents a 36-bit number. If the literal is a floating-point or alphanumeric number for DU or DL modifier variations, bits 0-17 of the literal are stored in the address field. In the case of all other literals, bits 18-35 of the literal are stored in the address field.
                             */
                            
                            //XXX this will fail for an alphanumeric literal "=1a," or "=1h ". Figure out a work-around
                            
                            arg = get18(arg1, mod);
                            goto process; // XXX gots ta' be a better way than stinkin' goto's
                        }
                    } else
                        mod = 0;    // I think
                }
                literal *l = doLiteral(arg1, addr);
                if (l)
                    arg = l->addr & AMASK;
                else
                    fprintf(stderr, "INTERNAL ERROR: literal *l == NULL @ line %d\n", lineno);
            }
            
            else if (nPass == 2)
            {
                // is arg1 a PRn|xxx ???
                char *v = strchr(arg1, '|');
                if (v)  // a pointer register
                    arg = encodePR(arg1, &bit29);
                else
                    arg = Eval(arg1) & 0777777;
            }
        }
        else
            arg = 0;
  
        if (arg2)
        {
            if (!(o->flags & NO_TAG))   // mod is *not* a tag?
                mod = getmod(arg2);
            else
                mod = boolEval(arg2) & 077;
        }
        else
            mod = 0;
        
process:;
        if (oct) ///<        addr         arg addline  mod  inLine
                 // fprintf(oct, "%6.6lo xxxx %6.6lo%3.3lo0%2.2o %s\n", addr++, arg, addline(op, arg1, arg2), mod, inLine);   //
            fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", addr++, arg, getOpcode(op) | bit29, mod, inLine);   //
        else
            addr++;
        
        
        /// free up memory from parseLine()
free:
        if (label)
            free(label);
        if (op)
            free(op);
        if (arg0)
            free(arg0);
        //for(int i = 0 ; args[i] ; i++)
        for(int i = 0 ; i < sizeof(args) / sizeof(args[0]) ; i++)
            if (args[i])
                free(args[i]);
    }
    
//    // perform end of pass processing ...
//    if (nPass == 1)
//    {
//        // fill in literals
//        fillLiteralPool(&addr);
//        
//        // fill in ITS/link pairs
//        
//        linkAddr = addr;    // offset of linkage section
//        
//        fillExtRef(&addr);
//    }
//    else if (nPass == 2)
//    {
//        writeLiteralPool(oct, &addr);
//        writeExtRef(oct, &addr);
//    }
//    
}

char *srcFile = "";

extern char *outFile;  ///< NULL;

int asMain(int argc, char **argv) {

    FILE *src, *oct;

    src = fopen(argv[0], "r");
    if (!src)
    {
        fprintf(stderr, "cannot open %s for reading\n", argv[0]);
        exit(1);
    }
    
    inFile = argv[0];   // for LEXgets
    srcFile = strdup(inFile);
    
    char *bnIn = strdup(basename(srcFile));
    char *bnOut = strdup(basename(outFile));
    
    if (strcmp(bnIn, bnOut) == 0)
    {
        fprintf(stderr, "output file <%s> *may* over write input file <%s>.\n", bnIn, bnOut);
        exit(1);        
    }
    
    oct = fopen(outFile, "w");
    if (!oct)
    {
        fprintf(stderr, "cannot open %s for writing\n", outFile);
        exit(1);
    }
//    }
//    else
//    {
//        oct = fopen(argv[1], "w");
//        if (!oct)
//        {
//            fprintf(stderr, "cannot open %s for writing\n", argv[1]);
//            exit(1);
//        }
//    }
    
    initSymtab();
    initliteralPool();

    // pass 1 - just build symbol table
    pass(1, src, NULL);
    
    rewind(src);
    
    doInterpass(oct);   // do any special interpass processing

    // pass 2 - do code generation
    pass(2, src, oct);
    
    if (verbose)
    {
        dumpSymtab();
        dumpliteralPool();
        dumpextRef();
    }
    
    doPostpass(oct);   // do any special post-pass processing

    fclose(oct);
    fclose(src);
    
    //exit(0);
    return 0;
}
