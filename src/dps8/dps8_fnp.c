#include <stdio.h>
#include "dps8.h"
#include "dps8_fnp.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"
#include "fnpp.h"

static t_stat fnpShowConfig (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat fnpSetConfig (UNIT * uptr, int value, char * cptr, void * desc);
static t_stat fnpShowNUnits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat fnpSetNUnits (UNIT * uptr, int32 value, char * cptr, void * desc);

static int fnpIOMCmd (UNIT * unitp, pcw_t * p);
//static int fnpIOT (UNIT * unitp, dcw_t * dcwp, bool *  disc);

#define N_FNP_UNITS_MAX 16
#define N_FNP_UNITS 1 // default

static t_stat fnpSVC (UNIT *up);

static UNIT fnp_unit [N_FNP_UNITS_MAX] = {
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& fnpSVC, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL}
};

static DEBTAB fnpDT [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };

static MTAB fnpMod [] =
  {
    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "CONFIG",     /* print string */
      "CONFIG",         /* match string */
      fnpSetConfig,         /* validation routine */
      fnpShowConfig, /* display routine */
      NULL,          /* value descriptor */
      NULL   // help string
    },

    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      fnpSetNUnits, /* validation routine */
      fnpShowNUnits, /* display routine */
      "Number of FNP units in the system", /* value descriptor */
      NULL          // help
    },
    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
  };

#define FNP_UNIT_NUM(uptr) ((uptr) - fnp_unit)

static t_stat fnpReset (DEVICE * dptr);

DEVICE fnpDev = {
    "FNP",           /* name */
    fnp_unit,          /* units */
    NULL,             /* registers */
    fnpMod,           /* modifiers */
    N_FNP_UNITS,       /* #units */
    10,               /* address radix */
    31,               /* address width */
    1,                /* address increment */
    8,                /* data radix */
    9,                /* data width */
    NULL,             /* examine routine */
    NULL,             /* deposit routine */
    fnpReset,         /* reset routine */
    NULL,             /* boot routine */
    NULL,             /* attach routine */
    NULL,             /* detach routine */
    NULL,             /* context */
    DEV_DEBUG,        /* flags */
    0,                /* debug control flags */
    fnpDT,            /* debug flag names */
    NULL,             /* memory size change */
    NULL,             /* logical name */
    NULL,             // attach help
    NULL,             // help
    NULL,             // help context
    NULL,             // device description
};

static struct fnpUnitData
  {
//-    enum { no_mode, read_mode, write_mode, survey_mode } io_mode;
//-    uint8 * bufp;
//-    t_mtrlnt tbc; // Number of bytes read into buffer
//-    uint words_processed; // Number of Word36 processed from the buffer
//-    int rec_num; // track tape position
    uint mailboxAddress;
  } fnpUnitData [N_FNP_UNITS_MAX];

static struct
  {
    int iomUnitNum;
    int chan_num;
    int dev_code;
  } cables_from_ioms_to_fnp [N_FNP_UNITS_MAX];

static int findFNPUnit (int iomUnitNum, int chan_num, int dev_code)
  {
    for (int i = 0; i < N_FNP_UNITS_MAX; i ++)
      {
        if (iomUnitNum == cables_from_ioms_to_fnp [i] . iomUnitNum &&
            chan_num     == cables_from_ioms_to_fnp [i] . chan_num     &&
            dev_code     == cables_from_ioms_to_fnp [i] . dev_code)
          return i;
      }
    return -1;
  }

int lookupFnpsIomUnitNumber (int fnpUnitNum)
  {
    return cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum;
  }

void fnpInit(void)
  {
    memset(fnpUnitData, 0, sizeof(fnpUnitData));
    for (int i = 0; i < N_FNP_UNITS_MAX; i ++)
      cables_from_ioms_to_fnp [i] . iomUnitNum = -1;
    fnppInit ();
  }

static t_stat fnpReset (DEVICE * dptr)
  {
    for (int i = 0; i < (int) dptr -> numunits; i ++)
      {
        sim_cancel (& fnp_unit [i]);
      }
    fnppReset (dptr);
    return SCPE_OK;
  }

t_stat cableFNP (int fnpUnitNum, int iomUnitNum, int chan_num, int dev_code)
  {
    if (fnpUnitNum < 0 || fnpUnitNum >= (int) fnpDev . numunits)
      {
        sim_printf ("cableFNP: fnpUnitNum out of range <%d>\n", fnpUnitNum);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum != -1)
      {
        sim_printf ("cableFNP: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iomUnitNum, chan_num, dev_code, DEVT_DN355, chan_type_PSI, fnpUnitNum, & fnpDev, & fnp_unit [fnpUnitNum], fnpIOMCmd);
    if (rc)
      return rc;

    cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum = iomUnitNum;
    cables_from_ioms_to_fnp [fnpUnitNum] . chan_num = chan_num;
    cables_from_ioms_to_fnp [fnpUnitNum] . dev_code = dev_code;

    return SCPE_OK;
  }
 
static int fnpCmd (UNIT * unitp, pcw_t * pcwp, bool * disc)
  {
    int fnpUnitNum = FNP_UNIT_NUM (unitp);
    int iomUnitNum = cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum;
    //struct fnpUnitData * p = & fnpUnitData [fnpUnitNum];
    * disc = false;

    int chan = pcwp-> chan;

    iomChannelData_ * chan_data = & iomChannelData [iomUnitNum] [chan];
    if (chan_data -> ptp)
      {
        sim_printf ("PTP in fnp; dev_cmd %o\n", pcwp -> dev_cmd);
        //sim_err ("PTP in fnp\n");
      }
    chan_data -> stati = 0;
sim_printf ("fnp cmd %d\n", pcwp -> dev_cmd);
    switch (pcwp -> dev_cmd)
      {
        case 000: // CMD 00 Request status
          {
sim_printf ("fnp cmd request status\n");
            chan_data -> stati = 04000;
            //disk_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & fnpDev, "Request status %d\n", fnpUnitNum);
            chan_data -> initiate = true;
          }
          break;

        default:
          {
            chan_data -> stati = 04501;
            sim_debug (DBG_ERR, & fnpDev,
                       "%s: Unknown command 0%o\n", __func__, pcwp -> dev_cmd);
            break;
          }
      }

    //status_service (iomUnitNum, chan, false);

    return 0;
  }

/*
 * fnpIOMCmd()
 *
 */

static int fnpIOMCmd (UNIT * unitp, pcw_t * pcwp)
  {
    int fnpUnitNum = FNP_UNIT_NUM (unitp);
    int iomUnitNum = cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum;
    struct fnpUnitData * p = & fnpUnitData [fnpUnitNum];

    // First, execute the command in the PCW, and then walk the 
    // payload channel mbx looking for IDCWs.

// Ignore a CMD 051 in the PCW
    if (pcwp -> dev_cmd == 051)
      return 1;
#if 0
sim_printf ("fnpIOMCmd\n");
sim_printf ("  [%lld]\n", sim_timell ());
sim_printf (" pcwp -> dev_cmd %02o\n", pcwp -> dev_cmd);
sim_printf (" pcwp -> dev_code %02o\n", pcwp -> dev_code);
sim_printf (" pcwp -> ext %02o\n", pcwp -> ext);
sim_printf (" pcwp -> cp %0o\n", pcwp -> cp);
sim_printf (" pcwp -> mask %0o\n", pcwp -> mask);
sim_printf (" pcwp -> control %0o\n", pcwp -> control);
sim_printf (" pcwp -> chan_cmd %0o\n", pcwp -> chan_cmd);
sim_printf (" pcwp -> chan_data %0o\n", pcwp -> chan_data);
sim_printf (" pcwp -> chan %0o\n", pcwp -> chan);
sim_printf (" pcwp -> ptPtr %0o\n", pcwp -> ptPtr);
sim_printf (" pcwp -> ptp %0o\n", pcwp -> ptp);
sim_printf (" pcwp -> pcw64_pge %0o\n", pcwp -> pcw64_pge);
sim_printf (" pcwp -> aux %0o\n", pcwp -> aux);
#endif
    bool disc;
//sim_printf ("1 st call to fnpCmd\n");
    fnpCmd (unitp, pcwp, & disc);

// peek at the mailbox

    word36 mbx0;
    fetch_abs_word (p -> mailboxAddress, & mbx0, "fnpIOMCmd");
    sim_printf ("mbx %08o:%012llo\n", p -> mailboxAddress, mbx0);

sim_printf ("end of list service; sending terminate interrupt\n");
    send_terminate_interrupt (iomUnitNum, pcwp -> chan);

    return 1;
  }

static t_stat fnpSVC (UNIT * unitp)
  {
    int fnpUnitNum = FNP_UNIT_NUM (unitp);
    int iomUnitNum = cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum;
    int chanNum = cables_from_ioms_to_fnp [fnpUnitNum] . chan_num;
    pcw_t * pcwp = & iomChannelData [iomUnitNum] [chanNum] . pcw;
    fnpIOMCmd (unitp, pcwp);
    return SCPE_OK;
  }
    

#if 0
static int fnpIOT (UNIT * unitp, dcw_t * dcwp, bool *  disc)
  {
    int fnpUnitNum = FNP_UNIT_NUM (unitp);
    int iomUnitNum = cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum;
    * disc = false;
    if (dcwp -> type == 0) // IOTD
      * disc = true;

  }

static int fnp_iom_io (UNIT * unitp, uint chan, uint dev_code, uint * tally, uint * cp, word36 * wordp, word12 * stati)
  {
    //sim_debug (DBG_DEBUG, & fnpDev, "%s\n", __func__);
    int fnpUnitNum = FNP_UNIT_NUM (unitp);
    //int iomUnitNum = cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum;
//--     
//--     int dev_unit_num;
//--     DEVICE* devp = get_iom_channel_dev (iomUnitNum, chan, dev_code, & dev_unit_num);
//--     if (devp == NULL || devp->units == NULL) {
//--         *majorp = 05;
//--         *subp = 2;
//--         sim_debug (DBG_ERR, &fnpDev, "fnp_iom_io: Internal error, no device and/or unit for channel 0%o\n", chan);
//--         return 1;
//--     }
//--     UNIT * unitp = & devp -> units [dev_unit_num];
//--     // BUG: no dev_code
//--     
    struct fnpUnitData * tape_statep = & fnpUnitData [fnpUnitNum];
    
    if (tape_statep -> io_mode == no_mode)
      {
        // no prior read or write command
        * stati = 05302; // MPC Device Data Alert Inconsistent command
        sim_debug (DBG_ERR, & fnpDev, "%s: Bad channel %d\n", __func__, chan);
        return 1;
      }
    else if (tape_statep -> io_mode == read_mode)
      {
        while (* tally)
          {
            // read
            if (extractWord36FromBuffer (tape_statep -> bufp, tape_statep -> tbc, & tape_statep -> words_processed, wordp) != 0)
              {
                // BUG: There isn't another word to be read from the tape buffer,
                // but the IOM wants  another word.
                // BUG: How did this tape hardware handle an attempt to read more
                // data than was present?
                // One answer is in bootload_tape_label.alm which seems to assume
                // a 4000 all-clear status.
                // Boot_tape_io.pl1 seems to assume that "short reads" into an
                // over-large buffer should not yield any error return.
                // So we'll set the flags to all-ok, but return an out-of-band
                // non-zero status to make the iom stop.
                // BUG: See some of the IOM status fields.
                // BUG: The IOM should be updated to return its DCW tally residue
                // to the caller.
                * stati = 04000;
                if (sim_tape_wrp (unitp))
                  * stati |= 1;
                sim_debug (DBG_WARN, & fnpDev,
                           "%s: Read buffer exhausted on channel %d\n",
                           __func__, chan);
                return 1;
              }
            wordp ++;
            (* tally) --;
          }
        * stati = 04000; // BUG: do we need to detect end-of-record?
        if (sim_tape_wrp (unitp))
          * stati |= 1;
        return 0;
      }
    else if (tape_statep -> io_mode == survey_mode)
      {
        //        2 survey_data,
        //          3 handler (16) unaligned,
        //            4 pad1 bit (1),               // 0
        //            4 reserved bit (1),           // 1
        //            4 operational bit (1),        // 2
        //            4 ready bit (1),              // 3
        //            4 number uns fixed bin (5),   // 4-8
        //            4 pad2 bit (1),               // 9
        //            4 speed uns fixed bin (3),    // 10-12
        //            4 nine_track bit (1),         // 13
        //            4 density uns fixed bin (4);  // 14-17
        
        return 0;
      }
    else
      {
        // write
        sim_debug (DBG_ERR, & fnpDev, "%s: Write I/O Unimplemented\n",
                   __func__);
        * stati = 04340; // Reflective end of tape mark found while trying to write
        return 1;
      }
    
//--     /*notreached*/
//--     *majorp = 0;
//--     *subp = 0;
//--     sim_debug (DBG_ERR, &fnpDev, "fnp_iom_io: Internal error.\n");
//--     cancel_run(STOP_BUG);
//    return 1;
  }
#endif

static t_stat fnpShowNUnits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                              UNUSED int val, UNUSED void * desc)
  {
    sim_printf("Number of FNO units in system is %d\n", fnpDev . numunits);
    return SCPE_OK;
  }

static t_stat fnpSetNUnits (UNUSED UNIT * uptr, UNUSED int32 value, 
                             char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_FNP_UNITS_MAX)
      return SCPE_ARG;
    fnpDev . numunits = (uint32) n;
    return fnppSetNunits (uptr, value, cptr, desc);
  }

static t_stat fnpShowConfig (UNUSED FILE * st, UNIT * uptr, UNUSED int val, 
                             UNUSED void * desc)
  {
    uint fnpUnitNum = FNP_UNIT_NUM (uptr);
    if (fnpUnitNum >= fnpDev . numunits)
      {
        sim_debug (DBG_ERR, & fnpDev, 
                   "fnpShowConfig: Invalid unit number %d\n", fnpUnitNum);
        sim_printf ("error: invalid unit number %u\n", fnpUnitNum);
        return SCPE_ARG;
      }

    sim_printf ("FNP unit number %u\n", fnpUnitNum);
    struct fnpUnitData * p = fnpUnitData + fnpUnitNum;

    sim_printf ("FNP Mailbox Address:         %04o(8)\n", p -> mailboxAddress);
#if 0
    char * os = "<out of range>";
    switch (p -> configSwOS)
      {
        case CONFIG_SW_STD_GCOS:
          os = "Standard GCOS";
          break;
        case CONFIG_SW_EXT_GCOS:
          os = "Extended GCOS";
          break;
        case CONFIG_SW_MULTICS:
          os = "Multics";
          break;
      }
    char * blct = "<out of range>";
    switch (p -> configSwBootloadCardTape)
      {
        case CONFIG_SW_BLCT_CARD:
          blct = "CARD";
          break;
        case CONFIG_SW_BLCT_TAPE:
          blct = "TAPE";
          break;
      }

    sim_printf ("Allowed Operating System: %s\n", os);
    sim_printf ("FNP Base Address:         %03o(8)\n", p -> configSwIomBaseAddress);
    sim_printf ("Multiplex Base Address:   %04o(8)\n", p -> configSwMultiplexBaseAddress);
    sim_printf ("Bootload Card/Tape:       %s\n", blct);
    sim_printf ("Bootload Tape Channel:    %02o(8)\n", p -> configSwBootloadMagtapeChan);
    sim_printf ("Bootload Card Channel:    %02o(8)\n", p -> configSwBootloadCardrdrChan);
    sim_printf ("Bootload Port:            %02o(8)\n", p -> configSwBootloadPort);
    sim_printf ("Port Address:            ");
    int i;
    for (i = 0; i < N_FNP_PORTS; i ++)
      sim_printf (" %03o", p -> configSwPortAddress [i]);
    sim_printf ("\n");
    sim_printf ("Port Interlace:          ");
    for (i = 0; i < N_FNP_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortInterface [i]);
    sim_printf ("\n");
    sim_printf ("Port Enable:             ");
    for (i = 0; i < N_FNP_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortEnable [i]);
    sim_printf ("\n");
    sim_printf ("Port Sysinit Enable:     ");
    for (i = 0; i < N_FNP_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortSysinitEnable [i]);
    sim_printf ("\n");
    sim_printf ("Port Halfsize:           ");
    for (i = 0; i < N_FNP_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortHalfsize [i]);
    sim_printf ("\n");
    sim_printf ("Port Storesize:           ");
    for (i = 0; i < N_FNP_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortStoresize [i]);
    sim_printf ("\n");
#endif
 
    return SCPE_OK;
  }

//
// set fnp0 config=<blah> [;<blah>]
//
//    blah = mailbox=n
//
//--//           multiplex_base=n
//--//           os=gcos | gcosext | multics
//-//---//           boot=card | tape
//--//           tapechan=n
//-//---//           cardchan=n
//--//           scuport=n
//--//           port=n   // set port number for below commands
//--//             addr=n
//--//             interlace=n
//--//             enable=n
//--//             initenable=n
//--//             halfsize=n
//--//             storesize=n
//--//          bootskip=n // Hack: forward skip n records after reading boot record

#if 0
static config_value_list_t cfg_os_list [] =
  {
    { "gcos", CONFIG_SW_STD_GCOS },
    { "gcosext", CONFIG_SW_EXT_GCOS },
    { "multics", CONFIG_SW_MULTICS },
    { NULL, 0 }
  };

static config_value_list_t cfg_boot_list [] =
  {
    { "card", CONFIG_SW_BLCT_CARD },
    { "tape", CONFIG_SW_BLCT_TAPE },
    { NULL, 0 }
  };

static config_value_list_t cfg_base_list [] =
  {
    { "multics", 014 },
    { "multics1", 014 }, // boot fnp
    { "multics2", 020 },
    { "multics3", 024 },
    { "multics4", 030 },
    { NULL, 0 }
  };

static config_value_list_t cfg_size_list [] =
  {
    { "32", 0 },
    { "64", 1 },
    { "128", 2 },
    { "256", 3 },
    { "512", 4 },
    { "1024", 5 },
    { "2048", 6 },
    { "4096", 7 },
    { "32K", 0 },
    { "64K", 1 },
    { "128K", 2 },
    { "256K", 3 },
    { "512K", 4 },
    { "1024K", 5 },
    { "2048K", 6 },
    { "4096K", 7 },
    { "1M", 5 },
    { "2M", 6 },
    { "4M", 7 },
    { NULL, 0 }
  };
#endif

static config_list_t fnp_config_list [] =
  {
    /*  0 */ { "mailbox", 0, 07777, NULL },
#if 0
    /*  0 */ { "os", 1, 0, cfg_os_list },
    /*  1 */ { "boot", 1, 0, cfg_boot_list },
    /*  2 */ { "fnp_base", 0, 07777, cfg_base_list },
    /*  3 */ { "multiplex_base", 0, 0777, NULL },
    /*  4 */ { "tapechan", 0, 077, NULL },
    /*  5 */ { "cardchan", 0, 077, NULL },
    /*  6 */ { "scuport", 0, 07, NULL },
    /*  7 */ { "port", 0, N_FNP_PORTS - 1, NULL },
    /*  8 */ { "addr", 0, 7, NULL },
    /*  9 */ { "interlace", 0, 1, NULL },
    /* 10 */ { "enable", 0, 1, NULL },
    /* 11 */ { "initenable", 0, 1, NULL },
    /* 12 */ { "halfsize", 0, 1, NULL },
    /* 13 */ { "store_size", 0, 7, cfg_size_list },
#endif

    { NULL, 0, 0, NULL }
  };

static t_stat fnpSetConfig (UNIT * uptr, UNUSED int value, char * cptr, UNUSED void * desc)
  {
    uint fnpUnitNUm = FNP_UNIT_NUM (uptr);
    if (fnpUnitNUm >= fnpDev . numunits)
      {
        sim_debug (DBG_ERR, & fnpDev, "fnpSetConfig: Invalid unit number %d\n", fnpUnitNUm);
        sim_printf ("error: fnpSetConfig: invalid unit number %d\n", fnpUnitNUm);
        return SCPE_ARG;
      }

    struct fnpUnitData * p = fnpUnitData + fnpUnitNUm;

    static uint port_num = 0;

    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("fnpSetConfig", cptr, fnp_config_list, & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 

            case -1: // done
              break;
            case 0: // OS
              p -> mailboxAddress = v;
              break;

#if 0
            case 1: // BOOT
              p -> configSwBootloadCardTape = v;
              break;

            case 2: // FNP_BASE
              p -> configSwIomBaseAddress = v;
              break;

            case 3: // MULTIPLEX_BASE
              p -> configSwMultiplexBaseAddress = v;
              break;

            case 4: // TAPECHAN
              p -> configSwBootloadMagtapeChan = v;
              break;

            case 5: // CARDCHAN
              p -> configSwBootloadCardrdrChan = v;
              break;

            case 6: // SCUPORT
              p -> configSwBootloadPort = v;
              break;

            case 7: // PORT
              port_num = v;
              break;

#if 0
                // all of the remaining assume a valid value in port_num
                if (/* port_num < 0 || */ port_num > 7)
                  {
                    sim_debug (DBG_ERR, & fnpDev, "fnpSetConfig: cached PORT value out of range: %d\n", port_num);
                    sim_printf ("error: fnpSetConfig: cached PORT value out of range: %d\n", port_num);
                    break;
                  } 
#endif
            case 8: // ADDR
              p -> configSwPortAddress [port_num] = v;
              break;

            case 9: // INTERLACE
              p -> configSwPortInterface [port_num] = v;
              break;

            case 10: // ENABLE
              p -> configSwPortEnable [port_num] = v;
              break;

            case 11: // INITENABLE
              p -> configSwPortSysinitEnable [port_num] = v;
              break;

            case 12: // HALFSIZE
              p -> configSwPortHalfsize [port_num] = v;
              break;

            case 13: // STORE_SIZE
              p -> configSwPortStoresize [port_num] = v;
              break;
#endif

            default:
              sim_debug (DBG_ERR, & fnpDev, "fnpSetConfig: Invalid cfgparse rc <%d>\n", rc);
              sim_printf ("error: fnpSetConfig: invalid cfgparse rc <%d>\n", rc);
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }
