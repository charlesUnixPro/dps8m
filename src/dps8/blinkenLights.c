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
word36 * M;
#include "dps8_cpu.h"

#include "dps8_mp.h"
#include "shm.h"

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

multipassStats * multipassStatsPtr;

static GdkRGBA lightOn, lightOff;

gboolean window_delete (GtkWidget * widget, cairo_t * cr, gpointer data)
  {
    //return true;
    exit (0);
  }

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

static bool E_state [8];
static bool X_state [8][18];
static bool IR_state [18];
static bool TR_state [27];
static bool RALR_state [3];

static bool SNR_state [8][15];
static bool RNR_state [8][3];
static bool BITNO_state [8][6];
static bool WORDNO_state [8][18];

static bool TRR_state [3];
static bool TSR_state [15];
static bool TBR_state [6];
static bool CA_state [18];

//static bool BASE_state [9];
//static bool BOUND_state [9];

static bool ADDR_state [24];
static bool BND_state [14];
static bool U_state [1];
static bool STACK_state [12];

static bool FAULT_SDF_state;
static bool FAULT_STR_state;
static bool FAULT_MME_state;
static bool FAULT_F1_state;
static bool FAULT_TRO_state;
static bool FAULT_CMD_state;
static bool FAULT_DRL_state;
static bool FAULT_LUF_state;
static bool FAULT_CON_state;
static bool FAULT_PAR_state;
static bool FAULT_IPR_state;
static bool FAULT_ONC_state;
static bool FAULT_SUF_state;
static bool FAULT_OFL_state;
static bool FAULT_DIV_state;
static bool FAULT_EXF_state;
static bool FAULT_DF0_state;
static bool FAULT_DF1_state;
static bool FAULT_DF2_state;
static bool FAULT_DF3_state;
static bool FAULT_ACV_state;
static bool FAULT_MME2_state;
static bool FAULT_MME3_state;
static bool FAULT_MME4_state;
static bool FAULT_F2_state;
static bool FAULT_F3_state;
static bool FAULT_TRB_state;
//static bool FAULT_oob_state;

static bool IPR_ill_op_state;
static bool IPR_ill_mod_state;
static bool IPR_ill_slv_state;
static bool IPR_ill_proc_state;
static bool ONC_nem_state;
//static bool STR_oob_state;
static bool IPR_ill_dig_state;
static bool PAR_proc_paru_state;
static bool PAR_proc_parl_state;
static bool CON_con_a_state;
static bool CON_con_b_state;
static bool CON_con_c_state;
static bool CON_con_d_state;
static bool ONC_da_err_state;
static bool ONC_da_err2_state;
static bool PAR_cpar_dir_state;
static bool PAR_cpar_str_state;
static bool PAR_cpar_ia_state;
static bool PAR_cpar_blk_state;
static bool FAULT_subtype_port_a_state;
static bool FAULT_subtype_port_b_state;
static bool FAULT_subtype_port_c_state;
static bool FAULT_subtype_port_d_state;
static bool FAULT_subtype_cpd_state;
static bool FAULT_subtype_level_0_state;
static bool FAULT_subtype_level_1_state;
static bool FAULT_subtype_level_2_state;
static bool FAULT_subtype_level_3_state;
static bool FAULT_subtype_cdd_state;
static bool FAULT_subtype_par_sdwam_state;
static bool FAULT_subtype_par_ptwam_state;
//static bool ACV_ACDF0_state;
//static bool ACV_ACDF1_state;
//static bool ACV_ACDF2_state;
//static bool ACV_ACDF3_state;
static bool ACV0_IRO_state;
static bool ACV1_OEB_state;
static bool ACV2_E_OFF_state;
static bool ACV3_ORB_state;
static bool ACV4_R_OFF_state;
static bool ACV5_OWB_state;
static bool ACV6_W_OFF_state;
static bool ACV7_NO_GA_state;
static bool ACV8_OCB_state;
static bool ACV9_OCALL_state;
static bool ACV10_BOC_state;
static bool ACV11_INRET_state;
static bool ACV12_CRT_state;
static bool ACV13_RALR_state;
static bool ACV_AME_state;
static bool ACV15_OOSB_state;

static bool intr_pair_addr_state [6];

static GtkWidget * PPR_display;
static GtkWidget * inst_display;
static GtkWidget * A_display;
static GtkWidget * Q_display;
static GtkWidget * E_display;
static GtkWidget * X_display[8];
static GtkWidget * IR_display;
static GtkWidget * TR_display;
static GtkWidget * RALR_display;
static GtkWidget * PAR_display[8];
//static GtkWidget * BAR_STRfault_display;
static GtkWidget * TPR_display;
static GtkWidget * DSBR_display;
static GtkWidget * fault_display[2];
static GtkWidget * IPRfault_display;
static GtkWidget * ONCfault_display;
static GtkWidget * PARfault_display;
static GtkWidget * CONfault_display;
static GtkWidget * cachefault_display;
//static GtkWidget * ACDF_display;
static GtkWidget * ACVfault_display;
static GtkWidget * intrpair_display;

static struct multipassStats previous;
//static struct _ppr previous_PPR;
//static word36 previous_inst = ~0llu;

//static pid_t ppid;
static pid_t sid;

static gboolean time_handler (GtkWidget * widget)
  {
    //if (ppid != getppid ())
      //exit (0);

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

    if (memcmp (& multipassStatsPtr -> E, & previous . E, sizeof (previous . E)))
      {
        update = true;
        for (int i = 0; i < 8; i ++)
          {
            E_state [7 - i] = ((1llu << i) & multipassStatsPtr -> E) ? 1 : 0;
          }
        //gtk_widget_queue_draw (E_display);
      }
    
    for(int nreg = 0; nreg < 8; nreg ++) {
      if (memcmp (& multipassStatsPtr -> X[nreg], & previous . X[nreg], sizeof (previous . X[nreg])))
        {
          update = true;
          for (int i = 0; i < 18; i ++)
            {
              X_state [nreg][17 - i] = ((1llu << i) & multipassStatsPtr -> X[nreg]) ? 1 : 0;
            }
          //gtk_widget_queue_draw (X_display[nreg]);
        }
    }



    if (memcmp (& multipassStatsPtr -> IR, & previous . IR, sizeof (previous . IR)))
      {
        update = true;
        for (int i = 0; i < 18; i ++)
          {
            IR_state [17 - i] = ((1llu << i) & multipassStatsPtr -> IR) ? 1 : 0;
          }
        //gtk_widget_queue_draw (IR_display);
      }

    if (memcmp (& multipassStatsPtr -> TR, & previous . TR, sizeof (previous . TR)))
      {
        update = true;
        for (int i = 0; i < 26; i ++)
          {
            TR_state [26 - i] = ((1llu << i) & multipassStatsPtr -> TR) ? 1 : 0;
          }
        //gtk_widget_queue_draw (TR_display);
      }

    if (memcmp (& multipassStatsPtr -> RALR, & previous . RALR, sizeof (previous . RALR)))
      {
        update = true;
        for (int i = 0; i < 3; i ++)
          {
            RALR_state [2 - i] = ((1llu << i) & multipassStatsPtr -> RALR) ? 1 : 0;
          }
        //gtk_widget_queue_draw (RALR_display);
      }

    for(int nreg = 0; nreg < 8; nreg ++) {
      if (memcmp (& multipassStatsPtr -> PAR[nreg], & previous . PAR[nreg], sizeof (previous . PAR[nreg])))
        {
          update = true;
          for (int i = 0; i < 15; i ++)
              SNR_state [nreg][14 - i] = ((1llu << i) & multipassStatsPtr -> PAR[nreg] . SNR) ? 1 : 0;
          for (int i = 0; i < 3; i ++)
              RNR_state [nreg][2 - i] = ((1llu << i) & multipassStatsPtr -> PAR[nreg] . RNR) ? 1 : 0;
          for (int i = 0; i < 6; i ++)
              BITNO_state [nreg][5 - i] = ((1llu << i) & multipassStatsPtr -> PAR[nreg] . BITNO) ? 1 : 0;
          for (int i = 0; i < 18; i ++)
              WORDNO_state [nreg][17 - i] = ((1llu << i) & multipassStatsPtr -> PAR[nreg] . WORDNO) ? 1 : 0;
          //gtk_widget_queue_draw (PAR_display[nreg]);
        }
    }



//    if (memcmp (& multipassStatsPtr -> BAR, & previous . BAR, sizeof (previous . BAR)))
//      {
//        update = true;
//        for (int i = 0; i < 9; i ++)
//            BASE_state [8 - i] = ((1llu << i) & multipassStatsPtr -> BAR . BASE) ? 1 : 0;
//        for (int i = 0; i < 9; i ++)
//            BOUND_state [8 - i] = ((1llu << i) & multipassStatsPtr -> BAR . BOUND) ? 1 : 0;
//        //gtk_widget_queue_draw (BAR_STRfault_display);
//      }


    if (memcmp (& multipassStatsPtr -> TRR, & previous . TRR, sizeof (previous . TRR)))
      {
        update = true;
        for (int i = 0; i < 3; i ++)
          {
            TRR_state [2 - i] = ((1llu << i) & multipassStatsPtr -> TRR) ? 1 : 0;
          }
        //gtk_widget_queue_draw (TPR_display);
      }

    if (memcmp (& multipassStatsPtr -> TSR, & previous . TSR, sizeof (previous . TSR)))
      {
        update = true;
        for (int i = 0; i < 15; i ++)
          {
            TSR_state [14 - i] = ((1llu << i) & multipassStatsPtr -> TSR) ? 1 : 0;
          }
        //gtk_widget_queue_draw (TPR_display);
      }

    if (memcmp (& multipassStatsPtr -> TBR, & previous . TBR, sizeof (previous . TBR)))
      {
        update = true;
        for (int i = 0; i < 6; i ++)
          {
            TBR_state [5 - i] = ((1llu << i) & multipassStatsPtr -> TBR) ? 1 : 0;
          }
        //gtk_widget_queue_draw (TPR_display);
      }

    if (memcmp (& multipassStatsPtr -> CA, & previous . CA, sizeof (previous . CA)))
      {
        update = true;
        for (int i = 0; i < 18; i ++)
          {
            CA_state [17 - i] = ((1llu << i) & multipassStatsPtr -> CA) ? 1 : 0;
          }
        //gtk_widget_queue_draw (TPR_display);
      }

    if (memcmp (& multipassStatsPtr -> DSBR, & previous . DSBR, sizeof (previous . DSBR)))
      {
        update = true;
        for (int i = 0; i < 24; i ++)
            ADDR_state [23 - i] = ((1llu << i) & multipassStatsPtr -> DSBR . ADDR) ? 1 : 0;
        for (int i = 0; i < 14; i ++)
            BND_state [13 - i] = ((1llu << i) & multipassStatsPtr -> DSBR . BND) ? 1 : 0;
        for (int i = 0; i < 1; i ++)
            U_state [0 - i] = ((1llu << i) & multipassStatsPtr -> DSBR . U) ? 1 : 0;
        for (int i = 0; i < 12; i ++)
            STACK_state [11 - i] = ((1llu << i) & multipassStatsPtr -> DSBR . STACK) ? 1 : 0;
        //gtk_widget_queue_draw (DSBR_display);
      }

    if (memcmp (& multipassStatsPtr -> faultNumber, & previous . faultNumber, sizeof (previous . faultNumber)))
      {
        update = true;
        FAULT_SDF_state = (FAULT_SDF == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_STR_state = (FAULT_STR == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_MME_state = (FAULT_MME == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_F1_state = (FAULT_F1 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_TRO_state = (FAULT_TRO == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_CMD_state = (FAULT_CMD == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_DRL_state = (FAULT_DRL == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_LUF_state = (FAULT_LUF == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_CON_state = (FAULT_CON == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_PAR_state = (FAULT_PAR == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_IPR_state = (FAULT_IPR == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_ONC_state = (FAULT_ONC == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_SUF_state = (FAULT_SUF == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_OFL_state = (FAULT_OFL == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_DIV_state = (FAULT_DIV == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_EXF_state = (FAULT_EXF == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_DF0_state = (FAULT_DF0 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_DF1_state = (FAULT_DF1 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_DF2_state = (FAULT_DF2 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_DF3_state = (FAULT_DF3 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_ACV_state = (FAULT_ACV == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_MME2_state = (FAULT_MME2 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_MME3_state = (FAULT_MME3 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_MME4_state = (FAULT_MME4 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_F2_state = (FAULT_F2 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_F3_state = (FAULT_F3 == multipassStatsPtr -> faultNumber) ? 1 : 0;
        FAULT_TRB_state = (FAULT_TRB == multipassStatsPtr -> faultNumber) ? 1 : 0;
        //FAULT_oob_state = (oob_fault == multipassStatsPtr -> faultNumber) ? 1 : 0;

        //gtk_widget_queue_draw (fault_display[0]);
        //gtk_widget_queue_draw (fault_display[1]);
      }


    if (update)
      gtk_widget_queue_draw (widget);
    previous = * multipassStatsPtr;
    return TRUE;

  }

int main (int argc, char * argv [])
  {

    struct sigaction quit_action;
    quit_action . sa_handler = SIG_IGN;
    quit_action . sa_flags = SA_RESTART;
    sigaction (SIGQUIT, & quit_action, NULL);

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
    //ppid = getppid ();
     sid = getsid (0);
     if (argc > 1 && strlen (argv [1]))
       {
         char * end;
         long p = strtol (argv [1], & end, 0);
         if (* end == 0)
           {
             sid = p;
             argv [1] [0] = 0;
           }
       }

    multipassStatsPtr = (multipassStats *) open_shm ("multipass", sid, sizeof (multipassStats));
    if (! multipassStatsPtr)
      {
        perror ("multipass open_shm");
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

// E


    E_display = gtk_grid_new ();

    memset (E_state, 0, sizeof (E_state));

    GtkWidget * E_lights = createLightArray (8, E_state);

    GtkWidget * E_label = gtk_label_new ("E ");

    gtk_grid_attach (GTK_GRID (E_display), E_label,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (E_display), E_lights, 1, 0, 1, 1);


// X

    GtkWidget * X_lights[8];
    GtkWidget * X_label[8];
    for(int nreg = 0; nreg < 8; nreg ++) {
      char X_text[4] = "Xn ";
      X_display[nreg] = gtk_grid_new ();

      memset (X_state[nreg], 0, sizeof (X_state[nreg]));

      X_lights[nreg] = createLightArray (18, X_state[nreg]);

      snprintf(X_text, sizeof(X_text), "X%d ", nreg);
      X_label[nreg] = gtk_label_new (X_text);

      gtk_grid_attach (GTK_GRID (X_display[nreg]), X_label[nreg],  0, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (X_display[nreg]), X_lights[nreg], 1, 0, 1, 1);
    }

// IR


    IR_display = gtk_grid_new ();

    memset (IR_state, 0, sizeof (IR_state));

    GtkWidget * IR_lights = createLightArray (18, IR_state);

    GtkWidget * IR_label = gtk_label_new ("IR ");

    gtk_grid_attach (GTK_GRID (IR_display), IR_label,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (IR_display), IR_lights, 1, 0, 1, 1);


// TR


    TR_display = gtk_grid_new ();

    memset (TR_state, 0, sizeof (TR_state));

    GtkWidget * TR_lights = createLightArray (27, TR_state);

    GtkWidget * TR_label = gtk_label_new ("TR ");

    gtk_grid_attach (GTK_GRID (TR_display), TR_label,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (TR_display), TR_lights, 1, 0, 1, 1);


// RALR


    RALR_display = gtk_grid_new ();

    memset (RALR_state, 0, sizeof (RALR_state));

    GtkWidget * RALR_lights = createLightArray (3, RALR_state);

    GtkWidget * RALR_label = gtk_label_new ("RALR ");

    gtk_grid_attach (GTK_GRID (RALR_display), RALR_label,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (RALR_display), RALR_lights, 1, 0, 1, 1);



// PAR

    GtkWidget * SNR_lights[8];
    GtkWidget * RNR_lights[8];
    GtkWidget * BITNO_lights[8];
    GtkWidget * WORDNO_lights[8];

    GtkWidget * PAR_label[8];
    GtkWidget * SNR_label[8];
    GtkWidget * RNR_label[8];
    GtkWidget * BITNO_label[8];
    GtkWidget * WORDNO_label[8];
    
    for(int nreg = 0; nreg < 8; nreg ++) {
      char PAR_text[6] = "PARn ";

      PAR_display[nreg] = gtk_grid_new ();
      memset (SNR_state[nreg], 0, sizeof (SNR_state[nreg]));
      memset (RNR_state[nreg], 0, sizeof (RNR_state[nreg]));
      memset (BITNO_state[nreg], 0, sizeof (BITNO_state[nreg]));
      memset (WORDNO_state[nreg], 0, sizeof (WORDNO_state[nreg]));

      SNR_lights[nreg] = createLightArray (15, SNR_state[nreg]);
      RNR_lights[nreg] = createLightArray (3, RNR_state[nreg]);
      BITNO_lights[nreg] = createLightArray (1, BITNO_state[nreg]);
      WORDNO_lights[nreg] = createLightArray (18, WORDNO_state[nreg]);

      snprintf(PAR_text, sizeof(PAR_text), "PAR%d ", nreg);
      PAR_label[nreg] = gtk_label_new (PAR_text);
      SNR_label[nreg] = gtk_label_new (" SNR ");
      RNR_label[nreg] = gtk_label_new (" RNR ");
      BITNO_label[nreg] = gtk_label_new (" BITNO ");
      WORDNO_label[nreg] = gtk_label_new (" WORDNO ");

      gtk_grid_attach (GTK_GRID (PAR_display[nreg]), PAR_label[nreg],  0, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (PAR_display[nreg]), SNR_label[nreg],  1, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (PAR_display[nreg]), SNR_lights[nreg], 2, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (PAR_display[nreg]), RNR_label[nreg],  3, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (PAR_display[nreg]), RNR_lights[nreg], 4, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (PAR_display[nreg]), BITNO_label[nreg],    5, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (PAR_display[nreg]), BITNO_lights[nreg],   6, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (PAR_display[nreg]), WORDNO_label[nreg],   7, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (PAR_display[nreg]), WORDNO_lights[nreg],  8, 0, 1, 1);
  }

  fault_display[0] = gtk_grid_new ();
  fault_display[1] = gtk_grid_new ();

  memset (&FAULT_SDF_state, 0, sizeof (FAULT_SDF_state));
  GtkWidget * FAULT_SDF_lights = createLightArray (1, &FAULT_SDF_state);
  GtkWidget * FAULT_SDF_label = gtk_label_new ("SDF ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_SDF_label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_SDF_lights, 1, 0, 1, 1);

  memset (&FAULT_STR_state, 0, sizeof (FAULT_STR_state));
  GtkWidget * FAULT_STR_lights = createLightArray (1, &FAULT_STR_state);
  GtkWidget * FAULT_STR_label = gtk_label_new ("STR ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_STR_label, 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_STR_lights, 3, 0, 1, 1);

  memset (&FAULT_MME_state, 0, sizeof (FAULT_MME_state));
  GtkWidget * FAULT_MME_lights = createLightArray (1, &FAULT_MME_state);
  GtkWidget * FAULT_MME_label = gtk_label_new ("MME ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_MME_label, 4, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_MME_lights, 5, 0, 1, 1);

  memset (&FAULT_F1_state, 0, sizeof (FAULT_F1_state));
  GtkWidget * FAULT_F1_lights = createLightArray (1, &FAULT_F1_state);
  GtkWidget * FAULT_F1_label = gtk_label_new ("F1 ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_F1_label, 6, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_F1_lights, 7, 0, 1, 1);

  memset (&FAULT_TRO_state, 0, sizeof (FAULT_TRO_state));
  GtkWidget * FAULT_TRO_lights = createLightArray (1, &FAULT_TRO_state);
  GtkWidget * FAULT_TRO_label = gtk_label_new ("TRO ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_TRO_label, 8, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_TRO_lights, 9, 0, 1, 1);

  memset (&FAULT_CMD_state, 0, sizeof (FAULT_CMD_state));
  GtkWidget * FAULT_CMD_lights = createLightArray (1, &FAULT_CMD_state);
  GtkWidget * FAULT_CMD_label = gtk_label_new ("CMD ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_CMD_label, 10, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_CMD_lights, 11, 0, 1, 1);

  memset (&FAULT_DRL_state, 0, sizeof (FAULT_DRL_state));
  GtkWidget * FAULT_DRL_lights = createLightArray (1, &FAULT_DRL_state);
  GtkWidget * FAULT_DRL_label = gtk_label_new ("DRL ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_DRL_label, 12, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_DRL_lights, 13, 0, 1, 1);

  memset (&FAULT_LUF_state, 0, sizeof (FAULT_LUF_state));
  GtkWidget * FAULT_LUF_lights = createLightArray (1, &FAULT_LUF_state);
  GtkWidget * FAULT_LUF_label = gtk_label_new ("LUF ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_LUF_label, 14, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_LUF_lights, 15, 0, 1, 1);

  memset (&FAULT_CON_state, 0, sizeof (FAULT_CON_state));
  GtkWidget * FAULT_CON_lights = createLightArray (1, &FAULT_CON_state);
  GtkWidget * FAULT_CON_label = gtk_label_new ("CON ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_CON_label, 16, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_CON_lights, 17, 0, 1, 1);

  memset (&FAULT_PAR_state, 0, sizeof (FAULT_PAR_state));
  GtkWidget * FAULT_PAR_lights = createLightArray (1, &FAULT_PAR_state);
  GtkWidget * FAULT_PAR_label = gtk_label_new ("PAR ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_PAR_label, 18, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_PAR_lights, 19, 0, 1, 1);

  memset (&FAULT_IPR_state, 0, sizeof (FAULT_IPR_state));
  GtkWidget * FAULT_IPR_lights = createLightArray (1, &FAULT_IPR_state);
  GtkWidget * FAULT_IPR_label = gtk_label_new ("IPR ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_IPR_label, 20, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_IPR_lights, 21, 0, 1, 1);

  memset (&FAULT_ONC_state, 0, sizeof (FAULT_ONC_state));
  GtkWidget * FAULT_ONC_lights = createLightArray (1, &FAULT_ONC_state);
  GtkWidget * FAULT_ONC_label = gtk_label_new ("ONC ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_ONC_label, 22, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_ONC_lights, 23, 0, 1, 1);

  memset (&FAULT_SUF_state, 0, sizeof (FAULT_SUF_state));
  GtkWidget * FAULT_SUF_lights = createLightArray (1, &FAULT_SUF_state);
  GtkWidget * FAULT_SUF_label = gtk_label_new ("SUF ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_SUF_label, 24, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_SUF_lights, 25, 0, 1, 1);

  memset (&FAULT_OFL_state, 0, sizeof (FAULT_OFL_state));
  GtkWidget * FAULT_OFL_lights = createLightArray (1, &FAULT_OFL_state);
  GtkWidget * FAULT_OFL_label = gtk_label_new ("OFL ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_OFL_label, 26, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_OFL_lights, 27, 0, 1, 1);

  memset (&FAULT_DIV_state, 0, sizeof (FAULT_DIV_state));
  GtkWidget * FAULT_DIV_lights = createLightArray (1, &FAULT_DIV_state);
  GtkWidget * FAULT_DIV_label = gtk_label_new ("DIV ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_DIV_label, 28, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_DIV_lights, 29, 0, 1, 1);

  memset (&FAULT_EXF_state, 0, sizeof (FAULT_EXF_state));
  GtkWidget * FAULT_EXF_lights = createLightArray (1, &FAULT_EXF_state);
  GtkWidget * FAULT_EXF_label = gtk_label_new ("EXF ");
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_EXF_label, 30, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[0]), FAULT_EXF_lights, 31, 0, 1, 1);

  memset (&FAULT_DF0_state, 0, sizeof (FAULT_DF0_state));
  GtkWidget * FAULT_DF0_lights = createLightArray (1, &FAULT_DF0_state);
  GtkWidget * FAULT_DF0_label = gtk_label_new ("DF0 ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_DF0_label, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_DF0_lights, 1, 0, 1, 1);

  memset (&FAULT_DF1_state, 0, sizeof (FAULT_DF1_state));
  GtkWidget * FAULT_DF1_lights = createLightArray (1, &FAULT_DF1_state);
  GtkWidget * FAULT_DF1_label = gtk_label_new ("DF1 ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_DF1_label, 2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_DF1_lights, 3, 0, 1, 1);

  memset (&FAULT_DF2_state, 0, sizeof (FAULT_DF2_state));
  GtkWidget * FAULT_DF2_lights = createLightArray (1, &FAULT_DF2_state);
  GtkWidget * FAULT_DF2_label = gtk_label_new ("DF2 ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_DF2_label, 4, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_DF2_lights, 5, 0, 1, 1);

  memset (&FAULT_DF3_state, 0, sizeof (FAULT_DF3_state));
  GtkWidget * FAULT_DF3_lights = createLightArray (1, &FAULT_DF3_state);
  GtkWidget * FAULT_DF3_label = gtk_label_new ("DF3 ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_DF3_label, 6, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_DF3_lights, 7, 0, 1, 1);

  memset (&FAULT_ACV_state, 0, sizeof (FAULT_ACV_state));
  GtkWidget * FAULT_ACV_lights = createLightArray (1, &FAULT_ACV_state);
  GtkWidget * FAULT_ACV_label = gtk_label_new ("ACV ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_ACV_label, 8, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_ACV_lights, 9, 0, 1, 1);

  memset (&FAULT_MME2_state, 0, sizeof (FAULT_MME2_state));
  GtkWidget * FAULT_MME2_lights = createLightArray (1, &FAULT_MME2_state);
  GtkWidget * FAULT_MME2_label = gtk_label_new ("MME2 ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_MME2_label, 10, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_MME2_lights, 11, 0, 1, 1);

  memset (&FAULT_MME3_state, 0, sizeof (FAULT_MME3_state));
  GtkWidget * FAULT_MME3_lights = createLightArray (1, &FAULT_MME3_state);
  GtkWidget * FAULT_MME3_label = gtk_label_new ("MME3 ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_MME3_label, 12, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_MME3_lights, 13, 0, 1, 1);

  memset (&FAULT_MME4_state, 0, sizeof (FAULT_MME4_state));
  GtkWidget * FAULT_MME4_lights = createLightArray (1, &FAULT_MME4_state);
  GtkWidget * FAULT_MME4_label = gtk_label_new ("MME4 ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_MME4_label, 14, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_MME4_lights, 15, 0, 1, 1);

  memset (&FAULT_F2_state, 0, sizeof (FAULT_F2_state));
  GtkWidget * FAULT_F2_lights = createLightArray (1, &FAULT_F2_state);
  GtkWidget * FAULT_F2_label = gtk_label_new ("F2 ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_F2_label, 16, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_F2_lights, 17, 0, 1, 1);

  memset (&FAULT_F3_state, 0, sizeof (FAULT_F3_state));
  GtkWidget * FAULT_F3_lights = createLightArray (1, &FAULT_F3_state);
  GtkWidget * FAULT_F3_label = gtk_label_new ("F3 ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_F3_label, 18, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_F3_lights, 19, 0, 1, 1);

  memset (&FAULT_TRB_state, 0, sizeof (FAULT_TRB_state));
  GtkWidget * FAULT_TRB_lights = createLightArray (1, &FAULT_TRB_state);
  GtkWidget * FAULT_TRB_label = gtk_label_new ("TRB ");
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_TRB_label, 20, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_TRB_lights, 21, 0, 1, 1);

//  memset (&FAULT_oob_state, 0, sizeof (FAULT_oob_state));
//  GtkWidget * FAULT_oob_lights = createLightArray (1, &FAULT_oob_state);
//  GtkWidget * FAULT_oob_label = gtk_label_new ("oob ");
//  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_oob_label, 22, 0, 1, 1);
//  gtk_grid_attach (GTK_GRID (fault_display[1]), FAULT_oob_lights, 23, 0, 1, 1);


// window_rows

    gtk_grid_attach (GTK_GRID (window_rows), PPR_display,  0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), inst_display, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), A_display,    0, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), Q_display,    0, 3, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), E_display,    0, 4, 1, 1);
    for(int nreg = 0; nreg < 8; nreg ++) {
      gtk_grid_attach (GTK_GRID (window_rows), X_display[nreg],    0, 5 + nreg, 1, 1);
    }
    gtk_grid_attach (GTK_GRID (window_rows), IR_display,    0, 13, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), TR_display,    0, 14, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), RALR_display,  0, 15, 1, 1);
    for(int nreg = 0; nreg < 8; nreg ++) {
      gtk_grid_attach (GTK_GRID (window_rows), PAR_display[nreg],    0, 16 + nreg, 1, 1);
    }
    gtk_grid_attach (GTK_GRID (window_rows), fault_display[0], 0, 24, 1, 1);
    gtk_grid_attach (GTK_GRID (window_rows), fault_display[1], 0, 25, 1, 1);

    gtk_container_add (GTK_CONTAINER (window), window_rows);

    // 100 = 10Hz
    // 10 = 100Hz
    g_timeout_add (10, (GSourceFunc) time_handler, (gpointer) window);

    g_signal_connect (window, "delete-event", window_delete, NULL);
    gtk_widget_show_all  (window);
    
    time_handler (window);

    gtk_main ();



    return 0;
  }

