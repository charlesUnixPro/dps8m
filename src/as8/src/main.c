/**
 * \file main.c
 * \project as8
 * \author Harry Reed
 * \date 10/20/12
 *  Created by Harry Reed on 10/20/12.
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "as.h"

int asMain(int argc, char **argv);

void usage()
{
    fprintf(stderr, "usage: as8 [-dv][-I include path][-o outputFile] program.src\n");
}

int literalType = 2;            ///< all literals are unique
int callingConvention = 1;      ///< use Honeywell/Bull calling convention for Call/Save/Return

static struct option longopts[] = {
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

int pscanf(const char * input, const char * format, ... );

char *includePath = ""; 
char *outFile = "as8.oct";  // default output file

int main(int argc, const char * argv[])
{

    char ch;
    debug = false;
    
    while ((ch = getopt_long(argc, argv, "dvI:o:", longopts, NULL)) != -1) {
        switch (ch) {
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
                if (callingConvention == 2)
                {
                    fprintf(stderr,"only Honeywell/Bull calling conventions currently supported\n");
                    callingConvention = 1;
                }

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

    if (verbose && strlen(includePath) > 0)
        fprintf(stderr, "Include path: <%s>\n", includePath);

    
    // only one file name allowed. That's the input file
    if (argc != 1)
    {
        usage();
        exit(1);
    }


    if (debug) printf("argc: %d argv[0]:%s argv[1]:%s\n", argc, argv[0], argv[1]);
    
        
    return asMain(argc, argv);
}

