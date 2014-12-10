#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>


#include <gtk/gtk.h>

#include "dps8.h"
#include "dps8_cpu.h"

#include "dps8_mp.h"
#include "shm.h"

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

multipassStats * multipassStatsPtr;

static GdkRGBA lightOn, lightOff;

gboolean draw_callback (GtkWidget * widget, cairo_t * cr, gpointer data)
  {
    guint width, height;

    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);
    cairo_arc (cr,
               width / 2.0, height / 2.0,
               MIN (width, height) / 2.0 * 0.8,
               0, 2 * G_PI);
    //gtk_style_context_get_color (gtk_widget_get_style_context (widget),
                                 //0,
                                 //& color);
    gdk_cairo_set_source_rgba (cr, & lightOff);
    cairo_fill (cr);


    if ( * (bool *) data)
      {
        cairo_arc (cr,
                   width / 2.0, height / 2.0,
                   MIN (width, height) / 2.0 * 0.6,
                   0, 2 * G_PI);
        gdk_cairo_set_source_rgba (cr, & lightOn);
        cairo_fill (cr);
      }

    return FALSE;
  }

static GtkWidget * createLight (bool * state)
  {
    GtkWidget * drawing_area = gtk_drawing_area_new ();
    gtk_widget_set_size_request (drawing_area, 10, 10);
    g_signal_connect (G_OBJECT (drawing_area), "draw",
                      G_CALLBACK (draw_callback), state);
    return drawing_area;
  }

static GtkWidget * createLightArray (int n, bool state [])
  {
    GtkWidget * grid = gtk_grid_new (); // (1, n, TRUE);
    for (int i = 0; i < n; i ++)
      {
        GtkWidget * l = createLight (& state [i]);
        gtk_grid_attach (GTK_GRID (grid), l, i, 0, 1, 1);
      }
    return grid;
  }
        
static bool PRR_state [3];
static bool PSR_state [15];
static bool P_state [1];
static bool IC_state [18];
static bool A_state [36];
static bool Q_state [36];
static bool inst_state [36];

static GtkWidget * PPR_display;
static GtkWidget * inst_display;
static GtkWidget * A_display;
static GtkWidget * Q_display;

static struct multipassStats previous;
//static struct _ppr previous_PPR;
//static word36 previous_inst = ~0llu;

static gboolean time_handler (GtkWidget * widget)
  {
    bool update = false;
    if (memcmp (& multipassStatsPtr -> PPR, & previous . PPR, sizeof (previous . PPR)))
      {
        update = true;
        for (int i = 0; i < 3; i ++)
          PRR_state [2 - i] = ((1llu << i) & multipassStatsPtr -> PPR . PRR) ? 1 : 0;
        for (int i = 0; i < 15; i ++)
          PSR_state [14 - i] = ((1llu << i) & multipassStatsPtr -> PPR . PSR) ? 1 : 0;
        for (int i = 0; i < 1; i ++)
          P_state [0 - i] = ((1llu << i) & multipassStatsPtr -> PPR . P) ? 1 : 0;
        for (int i = 0; i < 18; i ++)
          IC_state  [17 - i] = ((1llu << (i +  9)) & multipassStatsPtr -> PPR . IC) ? 1 : 0;
        //gtk_widget_queue_draw (PPR_display);
      }

    if (memcmp (& multipassStatsPtr -> inst, & previous . inst, sizeof (previous . inst)))
      {
        update = true;
        for (int i = 0; i < 36; i ++)
          {
            inst_state [35 - i] = ((1llu << i) & multipassStatsPtr -> inst) ? 1 : 0;
          }
        //gtk_widget_queue_draw (inst_display);
      }

    if (memcmp (& multipassStatsPtr -> A, & previous . A, sizeof (previous . A)))
      {
        update = true;
        for (int i = 0; i < 36; i ++)
          {
            A_state [35 - i] = ((1llu << i) & multipassStatsPtr -> A) ? 1 : 0;
          }
        //gtk_widget_queue_draw (A_display);
      }

    if (memcmp (& multipassStatsPtr -> Q, & previous . Q, sizeof (previous . Q)))
      {
        update = true;
        for (int i = 0; i < 36; i ++)
          {
            Q_state [35 - i] = ((1llu << i) & multipassStatsPtr -> Q) ? 1 : 0;
          }
        //gtk_widget_queue_draw (Q_display);
//printf ("Q ");
//for (int i = 0; i < 36; i ++) printf ("%d", Q_state [i]);
//printf ("\n");

      }

    if (update)
      gtk_widget_queue_draw (widget);
    previous = * multipassStatsPtr;
    return TRUE;

  }

int main (int argc, char * argv [])
  {
#if 0
    //printf ("Session %d\n", getsid (0));
    int fd = shm_open ("/multipass", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
      {
        printf ("multipass shm_open fail %d\n", errno);
        return 1;
      }

    if (ftruncate (fd, sizeof (multipassStats)) == -1)
      {
        printf ("multipass ftruncate  fail %d\n", errno);
        return 1;
      }

    multipassStatsPtr = (multipassStats *) mmap (NULL, sizeof (multipassStats),
                                                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (multipassStatsPtr == MAP_FAILED)
      {
        printf ("multipass mmap  fail %d\n", errno);
        return 1;
      }
#else
    multipassStatsPtr = (multipassStats *) open_shm ("multipass", getppid (), sizeof (multipassStats));
    if (! multipassStatsPtr)
      {
        printf ("multipass open_shm  fail %d\n", errno);
        return 1;
      }

#endif
#if 0
    for (;;)
      {
        printf ("%06lo:%06lo\n", 
                (multipassStatsPtr -> PPR_PSR_IC >> 18) & 0777777,
                (multipassStatsPtr -> PPR_PSR_IC >>  0) & 0777777);
        sleep (1);
      }
#endif

    gdk_rgba_parse (& lightOn, "white");
    gdk_rgba_parse (& lightOff, "black");

    GtkWidget * window;
    
    gtk_init (& argc, & argv);
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    GtkWidget * window_rows = gtk_grid_new ();

// PPR

    PPR_display = gtk_grid_new ();

    memset (PRR_state, 0, sizeof (PRR_state));
    memset (PSR_state, 0, sizeof (PSR_state));
    memset (P_state, 0, sizeof (P_state));
    memset (IC_state, 0, sizeof (IC_state));

    GtkWidget * PRR_lights = createLightArray (3, PRR_state);
    GtkWidget * PSR_lights = createLightArray (15, PSR_state);
    GtkWidget * P_lights = createLightArray (1, P_state);
    GtkWidget * IC_lights = createLightArray (18, IC_state);

    GtkWidget * PPR_label = gtk_label_new ("PPR ");
    GtkWidget * PRR_label = gtk_label_new (" PRR ");
    GtkWidget * PSR_label = gtk_label_new (" PSR ");
    GtkWidget * P_label = gtk_label_new (" P ");
    GtkWidget * IC_label = gtk_label_new (" IC ");

    gtk_grid_attach (GTK_GRID (PPR_display), PPR_label,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (PPR_display), PRR_label,  1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (PPR_display), PRR_lights, 2, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (PPR_display), PSR_label,  3, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (PPR_display), PSR_lights, 4, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (PPR_display), P_label,    5, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (PPR_display), P_lights,   6, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (PPR_display), IC_label,   7, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (PPR_display), IC_lights,  8, 0, 1, 1);

// instr

    inst_display = gtk_grid_new ();

    memset (inst_state, 0, sizeof (inst_state));

    GtkWidget * inst_lights = createLightArray (36, inst_state);

    GtkWidget * inst_label = gtk_label_new ("INSTRUCTION ");

    gtk_grid_attach (GTK_GRID (inst_display), inst_label,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (inst_display), inst_lights, 1, 0, 1, 1);

// A

    A_display = gtk_grid_new ();

    memset (A_state, 0, sizeof (A_state));

    GtkWidget * A_lights = createLightArray (36, A_state);

    GtkWidget * A_label = gtk_label_new ("A ");

    gtk_grid_attach (GTK_GRID (A_display), A_label,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (A_display), A_lights, 1, 0, 1, 1);

// Q


    Q_display = gtk_grid_new ();

    memset (Q_state, 0, sizeof (Q_state));

    GtkWidget * Q_lights = createLightArray (36, Q_state);

    GtkWidget * Q_label = gtk_label_new ("Q ");

    gtk_grid_attach (GTK_GRID (Q_display), Q_label,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (Q_display), Q_lights, 1, 0, 1, 1);

// window_rows

    gtk_grid_attach (GTK_GRID (window_rows), PPR_display,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), inst_display, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), A_display,    0, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), Q_display,    0, 3, 1, 1);

    gtk_container_add (GTK_CONTAINER (window), window_rows);

    // 100 = 10Hz
    // 10 = 100Hz
    g_timeout_add (10, (GSourceFunc) time_handler, (gpointer) window);

    gtk_widget_show_all  (window);
    
    time_handler (window);

    gtk_main ();



    return 0;
  }

