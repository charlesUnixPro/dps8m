#include <curses.h>
#include <string.h>

typedef void (* emitFunc) (char ch, void * ctx);

int lineedit (char ch, char * buffer, size_t bufsz, emitFunc f, void * ctx)
  {
    size_t len = strlen (buffer);

    if (ch == '\b' || ch == 0177)  // backspace or del
      {
        if (len > 0)
          {
            f ('\b', ctx);
            f (' ', ctx);
            f ('\b', ctx);
            buffer [len - 1] = 0;
          }
      }
    else if (ch == '\r' || ch == '\n' || ch == '\f')  // CR, LF, FF
      {
        if (len < bufsz)
          {
            f (ch, ctx);
            buffer [len ++] = ch;
            buffer [len] = 0;
            return 1;
          }
      }
    else if (ch == 022)  // ^R
      {
#if 0
         // CSI Ps K  Erase in Line (EL).
         //             Ps = 0  -> Erase to Right (default).
         //             Ps = 1  -> Erase to Left.
         //             Ps = 2  -> Erase All.
         //  'esc[2K'  
         f ('\r', ctx);
         f ('\033', ctx);
         f ('[', ctx);
         f ('2', ctx);
         f ('K', ctx);
#endif
         f ('\r', ctx);
         f ('\n', ctx);
         for (int i = 0; i < len; i ++)
           f (buffer [i], ctx);
         return 0;
      }
    else
      {
        if (len < bufsz)
          {
            f (ch, ctx);
            buffer [len ++] = ch;
            buffer [len] = 0;
          }
      }
    return 0;
  }

static void emit (char ch, void * ctx)
  {
    addch (ch);
  }

int main (int argc, char * argv [])
  {
    char buf [4096];
    int rc;
    char ch;
    initscr ();
    cbreak ();
    noecho ();
    buf [0] = 0;
    do
      {
        ch = getch ();
        rc = lineedit (ch, buf, sizeof (buf), emit, NULL);
      }
    while (rc == 0);
    endwin ();
    printf ("\n<%s>\n", buf);
    return 0;
  }
