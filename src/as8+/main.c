//
//  main.cpp
//  as8+
//
//  Created by Harry Reed on 3/20/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <getopt.h>

#include "as.h"

int yylex(), yyparse();

bool debug = false;          ///< output debug info
bool verbose = false;        ///< verbose program output

char *outFile = "as8.oct";    // default output file
char *inFile = "";
char *includePath   = "";

void usage()
{
    fprintf(stderr, "usage: as8+ [-dv][-I include path][-o outputFile] program.src\n");    
}

int literalType = 2;            ///< all literals are unique
int callingConvention = 1;      ///< use Honeywell/Bull calling convention for Call/Save/Return

static const struct option longopts[] = {
    { "debug",             no_argument,      NULL,                  'd' },
    { "verbose",           no_argument,      NULL,                  'v' },
    { "commonLiterals",    no_argument,      &literalType,           1  },
    { "uniqueLiterals",    no_argument,      &literalType,           2  },
    { "IncludePath",       required_argument,NULL,                  'I' },
    { "OutputFile",        required_argument,NULL,                  'o' },
    { "Honeywell",         no_argument,      &callingConvention,     1  },
    { "Multics",           no_argument,      &callingConvention,     2  },
    { NULL,                0,                NULL,                   0  }
};



void dumpOpCodes();

int main(int argc, char **argv)
{
    //dumpOpCodes();
    //return 0;

    // insert code here...

    char ch;
    debug = false;
    
    while ((ch = getopt_long(argc, (char * const *)argv, "dvI:o:", longopts, NULL)) != -1) {        switch (ch) {
            case 'd':
                debug = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 0:
                if (literalType == 1)
                {
                    fprintf(stderr,"only 'uniqueLiterals' are currently supported\n");
                    literalType = 2;
                }
                //if (callingConvention == 2)
                //{
                //    fprintf(stderr,"only Honeywell/Bull calling conventions currently supported\n");
                //    callingConvention = 1;
                //}
                
                break;
            case 'I':   ///< include path
                includePath = strdup(optarg);
                //                char *token; int n = 1;
                //                while ((token = strsep(&includePath, ";")) != NULL)
                //                {
                //                    if (strlen(strchop(token)) == 0)
                //                        continue;
                //
                //                    fprintf(stderr, "%d %s\n", n++, token);
                //                }
                break;
                
            case 'o':   ///< Output File spec
                outFile = strdup(optarg);
                break;
                
            case '?':
            default:
                usage();
                return -1;
        }
    }
    argc -= optind;
    argv += optind;
    
    if (verbose)
        printf("Hello, World This is as8+! (build: %s %s)\n", __DATE__, __TIME__);
    

    // only one file name allowed. That's the input file
    if (argc != 1)
    {
        usage();
        exit(1);
    }

    if (verbose && strlen(includePath) > 0)
        fprintf(stderr, "Include path: <%s>\n", includePath);
    
    
    
    if (debug)
    {
        printf("argc: %d\n", argc);

        for(int i = 0 ; i < argc ; i+= 1)
            printf("    argv[%d]:<%s>\n", i, argv[i]);
    }
    
    return asMain(argc, argv);
}
