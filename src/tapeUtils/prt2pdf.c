/* ============================================================================================================================== */

/*
         1         2         3         4         5         6         7         8         9        10        11        12        13
123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012
*/

/*
 *
 * Copyright (c) 2006 John S. Urban, USA. (urbanjost @ comcast. net)
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * JOHN S. URBAN ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  JOHN S. URBAN DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 -----------------------------------------------------------------------------
Modified:
asa2pdf; John S. Urban, Apr 30, 2006

Needed to emulate an old ASA 60 line by 132 column lineprinter quicky with
output as a PDF file.

Tested with xpdf, gv/ghostview, and acroread (PC version) PDF interpreters.

-----------------------------------------------------------------------------
V1:
o Began with txt2pdf; Copyright 1998; P. G. Womack, Diss, Norfolk, UK.
  "Do what you like, but don't claim you wrote it."
o Added bar shading.
o user-settable gray scale value via environment variable IMPACT_GRAY
o placed Adobe-recommended "binary" flag at top of PDF file.
o changed sizes to simulate a 60 line by 132 column lineprinter.
o changed so obeys ASA carriage control (' 01+' in column 1).
-----------------------------------------------------------------------------
V2:
o added command line options for
   o margins, lines per page, page size
   o gray scale, dash code pattern, shade spacing
   o margin messages, font
-----------------------------------------------------------------------------
    Next time in:

     o expand tabs (can use expand(1))                                   (T  0.1)
     o 1/2 line feeds?                                                   (T  0.2)
     o bold Courier font on overprint?                                   (T  0.3)
     o underline? font color? shade color?                               (?  3.0)
     o pick a color for overprinted text
     o truncating lines can be done with fold(1)
     o expanding tabs can be done with expand(1)
     o cat -n to add line numbers ??
     o font names? "Times-Roman"
     o Swap left and right page margins for even-numbered pages.
     o Print a table of contents to the end of the output.
     o Intelligent color highlighting like enscript(1) or a2ps(1) or vim(1)?

     But, if someone wants to get that fancy they can use
     fpr(1)/asa(1)/nasa(1) to handle the ASA carriage control, and
     something like a2ps(1)/enscript(1) and ps2pdf(1) if they have them
     or build them for their system.                                     (?  0.0 -- 10.0)
    -----------------------------------------------------------------------------
 */
/* ============================================================================================================================== */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
char *gets(char *s);

/* ============================================================================================================================== */
#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))
#define ABS(x)          ((x) < 0 ? -(x) : (x))
/* ============================================================================================================================== */
/* size of printable area */
/* Default unit is 72 points per inch */
 float GLOBAL_PAGE_DEPTH;
 float GLOBAL_PAGE_WIDTH;
 float GLOBAL_PAGE_MARGIN_LEFT;
 float GLOBAL_PAGE_MARGIN_RIGHT;
 float GLOBAL_PAGE_MARGIN_TOP;
 float GLOBAL_PAGE_MARGIN_BOTTOM;
 float GLOBAL_UNIT_MULTIPLIER=72.0;
 int   GLOBAL_SHADE_STEP=2;
 static  char GLOBAL_DASHCODE[256];
 static  char GLOBAL_FONT[256];
 int GLOBAL_SHIFT = 0;
 int GLOBAL_LINECOUNT=0;
 int GLOBAL_PAGES=0;
 int GLOBAL_PAGECOUNT=0;
 int GLOBAL_LINENUMBERS = 0;

 float GLOBAL_LINES_PER_PAGE;

 float GLOBAL_GRAY_SCALE;

 static char GLOBAL_CENTER_TITLE[256];
 static char GLOBAL_LEFT_TITLE[256];
 float GLOBAL_TITLE_SIZE;

/* ============================================================================================================================== */
 int GLOBAL_ADD=0;
 int GLOBAL_VERSION = 2;
 float GLOBAL_LEAD_SIZE;
 float GLOBAL_FONT_SIZE;
 int GLOBAL_OBJECT_ID = 1;
 int GLOBAL_PAGE_TREE_ID;
 int GLOBAL_NUM_PAGES = 0;
 int GLOBAL_NUM_XREFS = 0;
 long *GLOBAL_XREFS = NULL;
 int GLOBAL_STREAM_ID, GLOBAL_STREAM_LEN_ID;
 long GLOBAL_STREAM_START;
 float GLOBAL_YPOS;

 typedef struct _PageList {

    struct _PageList *next;

        int page_id;

 } PageList;

 PageList *GLOBAL_PAGE_LIST = NULL;
 PageList **GLOBAL_INSERT_PAGE = &GLOBAL_PAGE_LIST;

/* ============================================================================================================================== */
 void store_page(int id){

   PageList *n = (PageList *)malloc(sizeof(*n));

   if(n == NULL) {
      fprintf(stderr,"Unable to allocate array for page %d.", GLOBAL_NUM_PAGES + 1);
      exit(1);
   }
   n->next = NULL;
   n->page_id = id;
   *GLOBAL_INSERT_PAGE = n;
   GLOBAL_INSERT_PAGE = &n->next;

   GLOBAL_NUM_PAGES++;
 }
/* ============================================================================================================================== */
 void start_object(int id){
        if(id >= GLOBAL_NUM_XREFS) {

           long *new_xrefs;
           int delta, new_num_xrefs;
           delta = GLOBAL_NUM_XREFS / 5;

           if(delta < 1000) {
              delta += 1000;
          }

           new_num_xrefs = GLOBAL_NUM_XREFS + delta;
           new_xrefs = (long *)malloc(new_num_xrefs * sizeof(*new_xrefs));

           if(new_xrefs == NULL) {
              fprintf(stderr, "Unable to allocate array for object %d.", id);
              exit(1);
           }

           memcpy(new_xrefs, GLOBAL_XREFS, GLOBAL_NUM_XREFS * sizeof(*GLOBAL_XREFS));
           free(GLOBAL_XREFS);
           GLOBAL_XREFS = new_xrefs;
           GLOBAL_NUM_XREFS = new_num_xrefs;

        }

        GLOBAL_XREFS[id] = ftell(stdout);
        printf("%d 0 obj\n", id);

 }
/* ============================================================================================================================== */
 void print_bars(){

        float x1;
        float y1;
        float height;
        float width;
        float step;

        fprintf(stdout,"%f g\n",GLOBAL_GRAY_SCALE); /* gray-scale value */
        /*
        * If you want to add color,
        * R G B rg where R G B are red, green, blue components
        * in range 0.0 to 1.0 sets fill color, "RG" sets line
        * color instead of fill color.
        *
        * 0.60 0.82 0.60 rg
        *
        * */

        fprintf(stdout,"%d i\n",1); /*  */

        x1=GLOBAL_PAGE_MARGIN_LEFT-0.1*GLOBAL_FONT_SIZE;
        height=GLOBAL_SHADE_STEP*GLOBAL_LEAD_SIZE;
        y1 = GLOBAL_PAGE_DEPTH - GLOBAL_PAGE_MARGIN_TOP - height- 0.22*GLOBAL_FONT_SIZE;
        width=GLOBAL_PAGE_WIDTH-GLOBAL_PAGE_MARGIN_LEFT-GLOBAL_PAGE_MARGIN_RIGHT;
        step=1.0;
        if(GLOBAL_DASHCODE[0] != '\0'){
           fprintf(stdout, "0 w [%s] 0 d\n",GLOBAL_DASHCODE); /* dash code array plus offset */
        }
         /*
         8.4.3.6       Line Dash Pattern

         The line dash pattern shall control the pattern of dashes and gaps used to stroke paths. It shall be specified by
         a dash array and a dash phase. The dash array's elements shall be numbers that specify the lengths of
         alternating dashes and gaps; the numbers shall be nonnegative and not all zero. The dash phase shall specify
         the distance into the dash pattern at which to start the dash. The elements of both the dash array and the dash
         phase shall be expressed in user space units.

         Before beginning to stroke a path, the dash array shall be cycled through, adding up the lengths of dashes and
         gaps. When the accumulated length equals the value specified by the dash phase, stroking of the path shall
         begin, and the dash array shall be used cyclically from that point onward. Table 56 shows examples of line
         dash patterns. As can be seen from the table, an empty dash array and zero phase can be used to restore the
         dash pattern to a solid line.

                                               Table 56 ­ Examples of Line Dash Patterns

                              Dash Array       Appearance                   Description
                              and Phase

                              [] 0                                          No dash; solid, unbroken lines

                              [3] 0                                         3 units on, 3 units off, ...

                              [2] 1                                         1 on, 2 off, 2 on, 2 off, ...

                              [2 1] 0                                       2 on, 1 off, 2 on, 1 off, ...

                              [3 5] 6                                       2 off, 3 on, 5 off, 3 on, 5 off, ...

                              [ 2 3 ] 11                                    1 on, 3 off, 2 on, 3 off, 2 on, ...


         Dashed lines shall wrap around curves and corners just as solid stroked lines do. The ends of each dash shall
         be treated with the current line cap style, and corners within dashes shall be treated with the current line join
         style. A stroking operation shall take no measures to coordinate the dash pattern with features of the path; it
         simply shall dispense dashes and gaps along the path in the pattern defined by the dash array.

         When a path consisting of several subpaths is stroked, each subpath shall be treated independently--that is,
         the dash pattern shall be restarted and the dash phase shall be reapplied to it at the beginning of each subpath.
         */

        while ( y1 >= (GLOBAL_PAGE_MARGIN_BOTTOM-height) ){
           if(GLOBAL_DASHCODE[0] ==  '\0'){
                /* a shaded bar */
                 fprintf(stdout,"%f %f %f %f re f\n",x1,y1,width,height);
                 step=2.0;
                /*
                 * x1 y1 m x2 y2 l S
                 * xxx w  # line width
                 fprintf(stdout,"0.6 0.8 0.6 RG\n %f %f m %f %f l S\n",x1,y1,x1+width,y1);
                 */
           }else{
                  fprintf(stdout, "%f %f m ", x1 ,y1);
                  fprintf(stdout, "%f %f l s\n",x1+width,y1);
           }
           y1=y1-step*height;
        }
        if(GLOBAL_DASHCODE[0] != '\0'){
           fprintf(stdout, "[] 0 d\n"); /* set dash pattern to solid line */
        }

        fprintf(stdout,"%d G\n",0); /* */
        fprintf(stdout,"%d g\n",0); /* gray-scale value */

 }
/* ============================================================================================================================== */
 void printstring(char *buffer){
 /* Print string as (escaped_string) where ()\ have a preceding \ character added */
        char c;
        putchar('(');
        if(GLOBAL_LINENUMBERS != 0){
        fprintf(stdout,"%6d ",GLOBAL_LINECOUNT);
        }
              while((c = *buffer++) != '\0') {
                    switch(c+GLOBAL_ADD) {
                       case '(':
                       case ')':
                       case '\\':
                          putchar('\\');
                    }
                    putchar(c+GLOBAL_ADD);
        }
        putchar(')');
 }
/* ============================================================================================================================== */
 void printme(float xvalue,float yvalue,char *string){
        //float charwidth;
        //float start;
        fprintf(stdout,"BT /F2 %f Tf %f %f Td",GLOBAL_TITLE_SIZE,xvalue,yvalue);
        printstring(string);
        fprintf(stdout," Tj ET\n");
 }
/* ============================================================================================================================== */
 void printme_top(){
        char IMPACT_TOP[256];
	char *varname;
        float charwidth;
        float xvalue;
        float yvalue;
	float text_size=20.0;
        if( (varname=getenv("IMPACT_TOP")) != (char *)NULL ){
           strncpy(IMPACT_TOP,varname,255);
           charwidth=text_size*0.60; /* assuming fixed-space font Courier-Bold */
           fprintf(stdout,"1.0 0.0 0.0 rg\n"); /* gray-scale value */
           yvalue=GLOBAL_PAGE_DEPTH-text_size;
           xvalue=GLOBAL_PAGE_MARGIN_LEFT
              +((GLOBAL_PAGE_WIDTH-GLOBAL_PAGE_MARGIN_LEFT-GLOBAL_PAGE_MARGIN_RIGHT)/2.0)
              -(strlen(IMPACT_TOP)*charwidth/2.0);

           fprintf(stdout,"BT /F2 %f Tf %f %f Td",text_size,xvalue,yvalue);
           printstring(IMPACT_TOP);
           fprintf(stdout," Tj ET\n");

           fprintf(stdout,"0.0 0.0 0.0 rg\n"); /* gray-scale value */
        }
 }
/* ============================================================================================================================== */
 void print_margin_label(){
     char  line[80];
     float charwidth;
     float start;
     int hold;
     hold=GLOBAL_LINENUMBERS;
     GLOBAL_LINENUMBERS=0;

     printme_top();

     if(GLOBAL_CENTER_TITLE[0] != 0 /*NULL*/ ){

        /* assuming fixed-space font Courier-Bold */
        charwidth=GLOBAL_TITLE_SIZE*0.60;
        start=GLOBAL_PAGE_MARGIN_LEFT
           +((GLOBAL_PAGE_WIDTH-GLOBAL_PAGE_MARGIN_LEFT-GLOBAL_PAGE_MARGIN_RIGHT)/2.0)
           -(strlen(GLOBAL_CENTER_TITLE)*charwidth/2.0);

        printme(start,GLOBAL_PAGE_DEPTH-GLOBAL_PAGE_MARGIN_TOP+0.12*GLOBAL_TITLE_SIZE,GLOBAL_CENTER_TITLE);
        printme(start,GLOBAL_PAGE_MARGIN_BOTTOM-GLOBAL_TITLE_SIZE,GLOBAL_CENTER_TITLE);
     }

     if(GLOBAL_PAGES != 0 ){

        sprintf(line,"Page %4d",GLOBAL_PAGECOUNT);
        start=GLOBAL_PAGE_WIDTH-GLOBAL_PAGE_MARGIN_RIGHT-(strlen(line)*charwidth); /* Right Justified */
        printme(start,GLOBAL_PAGE_DEPTH-GLOBAL_PAGE_MARGIN_TOP+0.12*GLOBAL_TITLE_SIZE,line);
        printme(start,GLOBAL_PAGE_MARGIN_BOTTOM-GLOBAL_TITLE_SIZE,line);
     }

     if(GLOBAL_LEFT_TITLE[0] != '\0' ){
        start=GLOBAL_PAGE_MARGIN_LEFT; /* Left justified */
        printme(start,GLOBAL_PAGE_DEPTH-GLOBAL_PAGE_MARGIN_TOP+0.12*GLOBAL_TITLE_SIZE,GLOBAL_LEFT_TITLE);
        printme(start,GLOBAL_PAGE_MARGIN_BOTTOM-GLOBAL_TITLE_SIZE,GLOBAL_LEFT_TITLE);
     }
     GLOBAL_LINENUMBERS=hold;

 }
/* ============================================================================================================================== */
 void start_page() {
   GLOBAL_STREAM_ID = GLOBAL_OBJECT_ID++;
   GLOBAL_STREAM_LEN_ID = GLOBAL_OBJECT_ID++;
   GLOBAL_PAGECOUNT++;
   start_object(GLOBAL_STREAM_ID);
   printf("<< /Length %d 0 R >>\n", GLOBAL_STREAM_LEN_ID);
   printf("stream\n");
   GLOBAL_STREAM_START = ftell(stdout);
   print_bars();
   print_margin_label();
   printf("BT\n/F0 %g Tf\n", GLOBAL_FONT_SIZE);
   GLOBAL_YPOS = GLOBAL_PAGE_DEPTH - GLOBAL_PAGE_MARGIN_TOP;
   printf("%g %g Td\n", GLOBAL_PAGE_MARGIN_LEFT, GLOBAL_YPOS);
   printf("%g TL\n", GLOBAL_LEAD_SIZE);
 }
/* ============================================================================================================================== */
 void end_page(){

    long stream_len;
    int page_id = GLOBAL_OBJECT_ID++;

    store_page(page_id);
    printf("ET\n");
    stream_len = ftell(stdout) - GLOBAL_STREAM_START;
    printf("endstream\nendobj\n");
    start_object(GLOBAL_STREAM_LEN_ID);
    printf("%ld\nendobj\n", stream_len);
    start_object(page_id);
    printf("<</Type/Page/Parent %d 0 R/Contents %d 0 R>>\nendobj\n", GLOBAL_PAGE_TREE_ID, GLOBAL_STREAM_ID);
 }
/* ============================================================================================================================== */
void increment_ypos(float mult){
   if (GLOBAL_YPOS < GLOBAL_PAGE_DEPTH - GLOBAL_PAGE_MARGIN_TOP ){  /* if not at top of page */
      GLOBAL_YPOS += GLOBAL_LEAD_SIZE*mult;
   }
}
/* ============================================================================================================================== */
void do_text ()
  {

    char buffer [8192];
    //char ASA;
    char c;
    int black;
    int i;

    start_page ();

    for(i = 0; i < GLOBAL_SHIFT; i ++)
      {
        buffer [i] = ' ';
      }

    while ((c = getchar ()) != EOF)
      {
        if (c == '\r') // print the buffer, do not advance
          {
            fprintf (stdout, "0 %f Td\n", GLOBAL_LEAD_SIZE);
            increment_ypos (1.0);
            goto printline;
          }
        if (c == '\n') // print the buffer. advance          
          {
            goto printline;
          }
        if (c == '\f') // print the buffer, advance to next page
          {
            goto printline;
          }
        // add the character to the buffer
        if (i < 8190)
          buffer [i ++] = c;
        continue;
       
printline:
        GLOBAL_LINECOUNT ++;

        black=0;
        GLOBAL_ADD=0;

        /* +1 for roundoff , using floating point point units */
        if (GLOBAL_YPOS <= (GLOBAL_PAGE_MARGIN_BOTTOM + 1) &&
            strlen(buffer) != 0 &&
            buffer [0] != '+')
          {
            end_page ();
            start_page ();
          }
        buffer [i ++] = 0;
        printstring (buffer);
        printf ("'\n");

        if (c == '\f')
          {
            if (GLOBAL_YPOS < GLOBAL_PAGE_DEPTH - GLOBAL_PAGE_MARGIN_TOP )
              {
                end_page();
                start_page();
              }
          }
        GLOBAL_YPOS -= GLOBAL_LEAD_SIZE;
        if(black != 0)
          {
            fprintf(stdout,"0.0 0.0 0.0 rg\n"); /* black text */
          }
        i = GLOBAL_SHIFT;
      }
#if 0
      while(gets(&buffer[GLOBAL_SHIFT]) != NULL) {
         GLOBAL_LINECOUNT++;

         black=0;
         GLOBAL_ADD=0;

         /* +1 for roundoff , using floating point point units */
         if(GLOBAL_YPOS <= (GLOBAL_PAGE_MARGIN_BOTTOM+1) && strlen(buffer) != 0 && buffer[0] != '+' ) {
            end_page();
            start_page();
         }

         if(strlen(buffer) == 0){ /* blank line */
            printf("T*\n");
         }else{
#if 0
            ASA=buffer[0];

            switch(ASA) {

            case '1':     /* start a new page before processing data on line */
               if (GLOBAL_YPOS < GLOBAL_PAGE_DEPTH - GLOBAL_PAGE_MARGIN_TOP ){
                  end_page();
                  start_page();
               }
            break;

            case '0':        /* put out a blank line before processing data on line */
                  printf("T*\n");
                  GLOBAL_YPOS -= GLOBAL_LEAD_SIZE;
            break;

            case '-':        /* put out two blank lines before processing data on line */
               printf("T*\n");
               GLOBAL_YPOS -= GLOBAL_LEAD_SIZE;
               GLOBAL_YPOS -= GLOBAL_LEAD_SIZE;
            break;

            case '+':        /* print at same y-position as previous line */
               fprintf(stdout,"0 %f Td\n",GLOBAL_LEAD_SIZE);
               increment_ypos(1.0);
            break;

            case 'R':        /* RED print at same y-position as previous line */
            case 'G':        /* GREEN print at same y-position as previous line */
            case 'B':        /* BLUE print at same y-position as previous line */
               if(ASA == 'R') fprintf(stdout,"1.0 0.0 0.0 rg\n"); /* red text */
               if(ASA == 'G') fprintf(stdout,"0.0 1.0 0.0 rg\n"); /* green text */
               if(ASA == 'B') fprintf(stdout,"0.0 0.0 1.0 rg\n"); /* blue text */
               black=1;
               fprintf(stdout,"0 %f Td\n",GLOBAL_LEAD_SIZE);
               increment_ypos(1.0);
            break;

            case 'H':        /* 1/2 line advance */
               fprintf(stdout,"0 %f Td\n",GLOBAL_LEAD_SIZE/2.0);
               increment_ypos(0.5);
            break;

            case 'r':        /* RED print */
            case 'g':        /* GREEN print */
            case 'b':        /* BLUE print */
               if(ASA == 'r') fprintf(stdout,"1.0 0.0 0.0 rg\n"); /* red text */
               if(ASA == 'g') fprintf(stdout,"0.0 1.0 0.0 rg\n"); /* green text */
               if(ASA == 'b') fprintf(stdout,"0.0 0.0 1.0 rg\n"); /* blue text */
               black=1;
            break;

            case '^':        /* print at same y-position as previous line like + but add 127 to character */
               printf("0 %f Td\n",GLOBAL_LEAD_SIZE);
               increment_ypos(1.0);
               GLOBAL_ADD=127;
            break;

            case '>':        /* Unknown */
            break;

            case '\f':       /* ctrl-L is a common form-feed character on Unix, but NOT ASA */
               end_page();
               start_page();
            break;

            case ' ':
            break;

            default:
               fprintf(stderr,"unknown ASA carriage control character %c\n",buffer[0]);
            break;

         }
#endif
         printstring(&buffer[1]);
         printf("'\n");

      }
      GLOBAL_YPOS -= GLOBAL_LEAD_SIZE;
      if(black != 0){
         fprintf(stdout,"0.0 0.0 0.0 rg\n"); /* black text */
      }

   }
#endif
   end_page();
}
/* ============================================================================================================================== */
void dopages(){
        int i, catalog_id, font_id0, font_id1;
        long start_xref;

        printf("%%PDF-1.0\n");

        /*
           Note: If a PDF file contains binary data, as most do , it is
           recommended that the header line be immediately followed by a
           comment line containing at least four binary characters--that is,
           characters whose codes are 128 or greater. This will ensure proper behavior of file
           transfer applications that inspect data near the beginning of a
           file to determine whether to treat the file's contents as text or as binary.
        */
        fprintf(stdout,"%%%c%c%c%c\n",128,129,130,131);
        fprintf(stdout,"%% PDF: Adobe Portable Document Format\n");


        GLOBAL_LEAD_SIZE=(GLOBAL_PAGE_DEPTH-GLOBAL_PAGE_MARGIN_TOP-GLOBAL_PAGE_MARGIN_BOTTOM)/GLOBAL_LINES_PER_PAGE;
        GLOBAL_FONT_SIZE=GLOBAL_LEAD_SIZE;

        GLOBAL_OBJECT_ID = 1;
        GLOBAL_PAGE_TREE_ID = GLOBAL_OBJECT_ID++;

        do_text();

        font_id0 = GLOBAL_OBJECT_ID++;
        start_object(font_id0);
        printf("<</Type/Font/Subtype/Type1/BaseFont/%s/Encoding/WinAnsiEncoding>>\nendobj\n",GLOBAL_FONT);

        font_id1 = GLOBAL_OBJECT_ID++;
        start_object(font_id1);
        printf("<</Type/Font/Subtype/Type1/BaseFont/%s/Encoding/WinAnsiEncoding>>\nendobj\n",GLOBAL_FONT);

        start_object(GLOBAL_PAGE_TREE_ID);

        printf("<</Type /Pages /Count %d\n", GLOBAL_NUM_PAGES);

        {
           PageList *ptr = GLOBAL_PAGE_LIST;
           printf("/Kids[\n");
           while(ptr != NULL) {
              printf("%d 0 R\n", ptr->page_id);
              ptr = ptr->next;
           }
           printf("]\n");
        }

        printf("/Resources<</ProcSet[/PDF/Text]/Font<</F0 %d 0 R\n", font_id0);
        printf("/F1 %d 0 R\n", font_id1);
        printf(" /F2<</Type/Font/Subtype/Type1/BaseFont/Courier-Bold/Encoding/WinAnsiEncoding >> >>\n");
        printf(">>/MediaBox [ 0 0 %g %g ]\n", GLOBAL_PAGE_WIDTH, GLOBAL_PAGE_DEPTH);
        printf(">>\nendobj\n");
        catalog_id = GLOBAL_OBJECT_ID++;
        start_object(catalog_id);
        printf("<</Type/Catalog/Pages %d 0 R>>\nendobj\n", GLOBAL_PAGE_TREE_ID);
        start_xref = ftell(stdout);
        printf("xref\n");
        printf("0 %d\n", GLOBAL_OBJECT_ID);
        printf("0000000000 65535 f \n");

        for(i = 1; i < GLOBAL_OBJECT_ID; i++){
           printf("%010ld 00000 n \n", GLOBAL_XREFS[i]);
        }

        printf("trailer\n<<\n/Size %d\n/Root %d 0 R\n>>\n", GLOBAL_OBJECT_ID, catalog_id);
        printf("startxref\n%ld\n%%%%EOF\n", start_xref);
 }
/* ============================================================================================================================== */
void showhelp(int itype){
switch (itype){
case 1:
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |NAME:                                                                         |\n");
   fprintf(stderr," |asa2pdf: A filter to convert text files with ASA carriage control to a PDF.   |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |SYNOPSIS:                                                                     |\n");
   fprintf(stderr," |   asa2pdf(1) reads input from standard input. The first character            |\n");
   fprintf(stderr," |   of each line is interpreted as a control character. Lines beginning with   |\n");
   fprintf(stderr," |   any character other than those listed in the ASA carriage-control          |\n");
   fprintf(stderr," |   characters table are interpreted as if they began with a blank,            |\n");
   fprintf(stderr," |   and an appropriate diagnostic appears on standard error. The first         |\n");
   fprintf(stderr," |   character of each line is not printed.                                     |\n");
   fprintf(stderr," |     +------------+-----------------------------------------------+           |\n");
   fprintf(stderr," |     | Character  |                                               |           |\n");
   fprintf(stderr," |     +------------+-----------------------------------------------+           |\n");
   fprintf(stderr," |     | +          | Do not advance; overstrike previous line.     |           |\n");
   fprintf(stderr," |     | blank      | Advance one line.                             |           |\n");
   fprintf(stderr," |     | null lines | Treated as if they started with a blank       |           |\n");
   fprintf(stderr," |     | 0          | Advance two lines.                            |           |\n");
   fprintf(stderr," |     | -          | Advance three lines (IBM extension).          |           |\n");
   fprintf(stderr," |     | 1          | Advance to top of next page.                  |           |\n");
   fprintf(stderr," |     | all others | Discarded (except for extensions listed below)|           |\n");
   fprintf(stderr," |     +------------+-----------------------------------------------+           |\n");
   fprintf(stderr," | Extensions                                                                   |\n");
   fprintf(stderr," |    H  Advance one-half line.                                                 |\n");
   fprintf(stderr," |    R  Do not advance; overstrike previous line. Use red text color           |\n");
   fprintf(stderr," |    G  Do not advance; overstrike previous line. Use green text color         |\n");
   fprintf(stderr," |    B  Do not advance; overstrike previous line. Use blue text color          |\n");
   fprintf(stderr," |    r  Advance one line. Use red text color                                   |\n");
   fprintf(stderr," |    g  Advance one line. Use green text color                                 |\n");
   fprintf(stderr," |    b  Advance one line. Use blue text color                                  |\n");
   fprintf(stderr," |    ^  Overprint but add 127 to the ADE value of the character                |\n");
   fprintf(stderr," |       (ie., use ASCII extended character set)                                |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |PRINTABLE PAGE AREA                                                           |\n");
   fprintf(stderr," !  The page size may be specified using -H for height, -W for width, and -u    |\n");
   fprintf(stderr," !  to indicate the points per unit (72 makes H and W in inches,                |\n");
   fprintf(stderr," !  1 is used when units are in font points). For example:                      |\n");
   fprintf(stderr," |    -u 72 -H 8.5 -W 11   # page Height and Width in inches                    |\n");
   fprintf(stderr," |    -u 72 -B 0.5 -L 0.5 -R 0.5 -T 0.5 # margins (Top, Bottom, Left, Right)    |\n");
   fprintf(stderr," |  common media sizes with -u 1:                                               |\n");
   fprintf(stderr," |    +-------------------+------+------------+                                 |\n");
   fprintf(stderr," |    | name              |  W   |        H   |                                 |\n");
   fprintf(stderr," |    +-------------------+------+------------+                                 |\n");
   fprintf(stderr," |    | Letterdj (11x8.5) | 792  |       612  | (LandScape)                     |\n");
   fprintf(stderr," |    | A4dj              | 842  |       595  |                                 |\n");
   fprintf(stderr," |    | Letter (8.5x11)   | 612  |       792  | (Portrait)                      |\n");
   fprintf(stderr," |    | Legal             | 612  |       1008 |                                 |\n");
   fprintf(stderr," |    | A5                | 420  |       595  |                                 |\n");
   fprintf(stderr," |    | A4                | 595  |       842  |                                 |\n");
   fprintf(stderr," |    | A3                | 842  |       1190 |                                 |\n");
   fprintf(stderr," |    +-------------------+------+------------+                                 |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |SHADING                                                                       |\n");
   fprintf(stderr," |    -g 0.800781     # gray-scale value  for shaded bars ( 0 < g 1 )           |\n");
   fprintf(stderr," |                    # 0 is black, 1 is white.                                 |\n");
   fprintf(stderr," |    -i 2            # repeat shade pattern every N lines                      |\n");
   fprintf(stderr," |    -d ' '          # dashcode pattern (seems buggy)                          |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |MARGIN LABELS                                                                 |\n");
   fprintf(stderr," |   -s ''            # top middle page label.                                  |\n");
   fprintf(stderr," |   -t ''            # top left page label.                                    |\n");
   fprintf(stderr," |   -P               # add page numbers to right corners                       |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |TEXT OPTIONS                                                                  |\n");
   fprintf(stderr," |   -l 60            # lines per page                                          |\n");
   fprintf(stderr," |   -f Courier       # font names: Courier, Courier-Bold,Courier-Oblique       |\n");
   fprintf(stderr," |                      Helvetica, Symbol, Times-Bold, Helvetica-Bold,          |\n");
   fprintf(stderr," |                      ZapfDingbats, Times-Italic, Helvetica-Oblique,          |\n");
   fprintf(stderr," |                      Times-BoldItalic, Helvetica-BoldOblique,                |\n");
   fprintf(stderr," |                      Times-Roman, Courier-BoldOblique                        |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |   -S 0             # right shift 1 for non-ASA files                         |\n");
   fprintf(stderr," |   -N               # add line numbers                                        |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |   -v 2             # version number                                          |\n");
   fprintf(stderr," |   -h               # display this help                                       |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |ENVIRONMENT VARIABLES:                                                        |\n");
   fprintf(stderr," | $IMPACT_TOP Will be printed in large red letters across the page top.        |\n");
   fprintf(stderr," | $IMPACT_GRAY sets the default gray-scale value, same as the -g switch.       |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");
   fprintf(stderr," |EXAMPLES:                                                                     |\n");
   fprintf(stderr," !-----------------                                                             |\n");
   fprintf(stderr," | # create non-ASA file in portrait mode with a dashed line under every line   |\n");
   fprintf(stderr," | asa2pdf -S 1 -W 8.5 -H 11 -i 1 -d '2 4 1' -T 1 -B .75 < INFILE > junko.pdf   |\n");
   fprintf(stderr," !-----------------                                                             |\n");
   fprintf(stderr," | # banner on top                                                              |\n");
   fprintf(stderr," | env IMPACT_GRAY=1 IMPACT_TOP=CONFIDENTIAL asa2pdf < test.txt > test.pdf      |\n");
   fprintf(stderr," !-----------------                                                             |\n");
   fprintf(stderr," | # 132 landscape                                                              |\n");
   fprintf(stderr," |  asa2pdf -s LANDSCAPE <asa2pdf.c >junko.A.pdf                                |\n");
   fprintf(stderr," !-----------------                                                             |\n");
   fprintf(stderr," | # 132 landscape with line numbers with dashed lines                          |\n");
   fprintf(stderr," |  asa2pdf -s 'LANDSCAPE LINE NUMBERS' -d '3 1 2' \\                            |\n");
   fprintf(stderr," |  -N -T .9 <asa2pdf.c >test.pdf                                               |\n");
   fprintf(stderr," !-----------------                                                             |\n");
   fprintf(stderr," | # portrait 80 non-ASA file with dashed lines                                 |\n");
   fprintf(stderr," |  asa2pdf -s PORTRAIT -S 1 -W 8.5 -H 11 -i 1 -d '2 4 1' \\                     |\n");
   fprintf(stderr," |  -T 1 -B .75 < asa2pdf.c > test.pdf                                          |\n");
   fprintf(stderr," !-----------------                                                             |\n");
   fprintf(stderr," | # portrait 80 with line numbers , non-ASA                                    |\n");
   fprintf(stderr," |  asa2pdf -s 'PORTRAIT LINE NUMBERS' -l 66 -S 1 -W 8.5 -H 11 \\                |\n");
   fprintf(stderr," |  -i 1 -T 1 -B .75 -N < asa2pdf.c > test.pdf                                  |\n");
   fprintf(stderr," !-----------------                                                             |\n");
   fprintf(stderr," | # titling                                                                    |\n");
   fprintf(stderr," |  asa2pdf -d '1 0 1' -t \"$USER\" -i 1 -P -N -T 1 \\                             |\n");
   fprintf(stderr," |  -s \"asa2pdf.c\" <asa2pdf.c >test.pdf                                         |\n");
   fprintf(stderr," +------------------------------------------------------------------------------+\n");

break;
case 2:
fprintf (stderr,"-T %f # Top margin\n", GLOBAL_PAGE_MARGIN_TOP/GLOBAL_UNIT_MULTIPLIER);
fprintf (stderr,"-B %f # Bottom margin\n", GLOBAL_PAGE_MARGIN_BOTTOM/GLOBAL_UNIT_MULTIPLIER);
fprintf (stderr,"-L %f # Left margin\n", GLOBAL_PAGE_MARGIN_LEFT/GLOBAL_UNIT_MULTIPLIER);
fprintf (stderr,"-R %f # Right margin\n", GLOBAL_PAGE_MARGIN_RIGHT/GLOBAL_UNIT_MULTIPLIER);

fprintf (stderr,"-W %f # page Width\n", GLOBAL_PAGE_WIDTH/GLOBAL_UNIT_MULTIPLIER);
fprintf (stderr,"-H %f # page Height\n", GLOBAL_PAGE_DEPTH/GLOBAL_UNIT_MULTIPLIER);

fprintf (stderr,"-u %f # unit multiplier\n", GLOBAL_UNIT_MULTIPLIER);

fprintf (stderr,"-g %f # shading gray scale value ([black]0 <= g <= 1[white]\n", GLOBAL_GRAY_SCALE);
fprintf (stderr,"-i %d # shading line increment\n", GLOBAL_SHADE_STEP);
fprintf (stderr,"-d %s # shading line dashcode\n", GLOBAL_DASHCODE);

fprintf (stderr,"-l %f # lines per page\n", GLOBAL_LINES_PER_PAGE);
fprintf (stderr,"-f %s # font name\n", GLOBAL_FONT);

fprintf (stderr,"-s %s # margin label\n", GLOBAL_CENTER_TITLE);
fprintf (stderr,"-t %s # margin left label\n", GLOBAL_LEFT_TITLE);
fprintf (stderr,"-S %d # right shift\n", GLOBAL_SHIFT);

fprintf (stderr,"-N [flag=%d]   # add line numbers \n", GLOBAL_LINENUMBERS);
fprintf (stderr,"-P [flag=%d] # add page numbers\n", GLOBAL_PAGES);

fprintf (stderr,"-v %d # version number\n", GLOBAL_VERSION);
fprintf (stderr,"-h    # display help\n");
break;
}
}
/* ============================================================================================================================== */
int main(int argc, char **argv) {
/*
   How getopt is typically used. The key points to notice are:
     * Normally, getopt is called in a loop. When getopt returns -1,
       indicating no more options are present, the loop terminates.
     * A switch statement is used to dispatch on the return value from
       getopt. In typical use, each case just sets a variable that is
       used later in the program.
     * A second loop is used to process the remaining non-option
       arguments.
*/

   char *varname;

       int index;
       int c;
   GLOBAL_PAGE_DEPTH =        612.0;
   GLOBAL_PAGE_WIDTH =        792.0;      /* Default is 72 points per inch */
   GLOBAL_PAGE_MARGIN_TOP =    36.0;
   GLOBAL_PAGE_MARGIN_BOTTOM = 36.0;
   GLOBAL_PAGE_MARGIN_LEFT =   40.0 -14.0;
   GLOBAL_PAGE_MARGIN_RIGHT =  39.0 -14.0;
   GLOBAL_LINES_PER_PAGE=      60.0;
   GLOBAL_GRAY_SCALE=           0.800781; /* gray-scale value */


   varname=getenv("IMPACT_GRAY");
   if (varname == (char*)NULL ){
       GLOBAL_GRAY_SCALE=0.800781; /* gray-scale value */
   }else if (varname[0] == '\0'){
       GLOBAL_GRAY_SCALE=0.800781; /* gray-scale value */
   }else{
      sscanf(varname,"%f",&GLOBAL_GRAY_SCALE);
      if(GLOBAL_GRAY_SCALE < 0 ){
          GLOBAL_GRAY_SCALE=0.800781; /* gray-scale value */
      }
   }

   GLOBAL_LEFT_TITLE[0]='\0';
   GLOBAL_CENTER_TITLE[0]='\0';


   opterr = 0;

   strncpy(GLOBAL_DASHCODE,"",255);
   strncpy(GLOBAL_FONT,"Courier",255);
   GLOBAL_TITLE_SIZE=20.0;

   while ((c = getopt (argc, argv, "B:d:f:g:H:hi:L:l:NPR:s:S:t:T:u:W:vX")) != -1)
         switch (c) {
           case 'L': GLOBAL_PAGE_MARGIN_LEFT =    strtod(optarg,NULL)*GLOBAL_UNIT_MULTIPLIER; break; /* Left margin              */
           case 'R': GLOBAL_PAGE_MARGIN_RIGHT =   strtod(optarg,NULL)*GLOBAL_UNIT_MULTIPLIER; break; /* Right margin             */
           case 'B': GLOBAL_PAGE_MARGIN_BOTTOM =  strtod(optarg,NULL)*GLOBAL_UNIT_MULTIPLIER; break; /* Bottom margin            */
           case 'T': GLOBAL_PAGE_MARGIN_TOP =     strtod(optarg,NULL)*GLOBAL_UNIT_MULTIPLIER; break; /* Top margin               */
           case 'H': GLOBAL_PAGE_DEPTH =          strtod(optarg,NULL)*GLOBAL_UNIT_MULTIPLIER; break; /* Height                   */
           case 'W': GLOBAL_PAGE_WIDTH =          strtod(optarg,NULL)*GLOBAL_UNIT_MULTIPLIER; break; /* Width                    */

           case 'g': GLOBAL_GRAY_SCALE =          strtod(optarg,NULL);                        break; /* grayscale value for bars */
           case 'l': GLOBAL_LINES_PER_PAGE=       strtod(optarg,NULL);                        break; /* lines per page           */
           case 'u': GLOBAL_UNIT_MULTIPLIER =     strtod(optarg,NULL);                        break; /* unit_divisor             */
           case 'i': GLOBAL_SHADE_STEP =          strtod(optarg,NULL);                        break; /* increment for bars       */
           case 'S': GLOBAL_SHIFT =               MAX(0,strtod(optarg,NULL));                 break; /* right shift              */

           case 's': strncpy(GLOBAL_CENTER_TITLE,optarg,255);                                 break; /* special label            */
           case 't': strncpy(GLOBAL_LEFT_TITLE,optarg,255);                                   break; /* margin left label        */

           case 'd': strncpy(GLOBAL_DASHCODE,optarg,255);                                     break; /* dash code                */
           case 'f': strncpy(GLOBAL_FONT,optarg,255);                                         break; /* font                     */

           case 'N': GLOBAL_LINENUMBERS=1;                                                    break; /* number lines             */
           case 'P': GLOBAL_PAGES=1;                                                          break; /* number pages             */
           case 'h': showhelp(1);exit(1);                                                     break; /* help                     */
           case 'X': showhelp(2);                                                             break;
           case 'v': fprintf (stderr, "asa2pdf version %d\n",GLOBAL_VERSION); exit(2);        break; /* version                  */

           case '?':
             fprintf(stderr," SWITCH IS %c\n",c);
             if (isprint (optopt)){
               fprintf (stderr, "Unknown option `-%c'.\n", optopt);
             }else{
               fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
             }
             showhelp(2);
             exit(1);
             return 1;

           default:
             abort ();
           }

           if(GLOBAL_SHADE_STEP < 1 ){
              fprintf(stderr,"W-A-R-N-I-N-G: asa2pdf(1) resetting -i %d to -i 1\n",GLOBAL_SHADE_STEP);
              GLOBAL_SHADE_STEP=1;
   }

   for (index = optind; index < argc; index++){
      fprintf (stderr,"Non-option argument %s\n", argv[index]);
   }
   dopages();
   exit(0);
}
/* ============================================================================================================================== */
