#include <stdio.h>
#include "dps8.h"
#include "dps8_fnp.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"

static t_stat fnpShowNUnits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat fnpSetNUnits (UNIT * uptr, int32 value, char * cptr, void * desc);
static int fnpIOMCmd (UNIT * unitp, pcw_t * p);

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

static struct fnpState
  {
//-    enum { no_mode, read_mode, write_mode, survey_mode } io_mode;
//-    uint8 * bufp;
//-    t_mtrlnt tbc; // Number of bytes read into buffer
//-    uint words_processed; // Number of Word36 processed from the buffer
//-    int rec_num; // track tape position
  } fnpState [N_FNP_UNITS_MAX];

static struct
  {
    int iom_unit_num;
    int chan_num;
    int dev_code;
  } cables_from_ioms_to_fnp [N_FNP_UNITS_MAX];

static int findFNPUnit (int iom_unit_num, int chan_num, int dev_code)
  {
    for (int i = 0; i < N_FNP_UNITS_MAX; i ++)
      {
        if (iom_unit_num == cables_from_ioms_to_fnp [i] . iom_unit_num &&
            chan_num     == cables_from_ioms_to_fnp [i] . chan_num     &&
            dev_code     == cables_from_ioms_to_fnp [i] . dev_code)
          return i;
      }
    return -1;
  }


void fnpInit(void)
  {
    memset(fnpState, 0, sizeof(fnpState));
    for (int i = 0; i < N_FNP_UNITS_MAX; i ++)
      cables_from_ioms_to_fnp [i] . iom_unit_num = -1;
  }

static t_stat fnpReset (DEVICE * dptr)
  {
    for (int i = 0; i < (int) dptr -> numunits; i ++)
      {
        sim_cancel (& fnp_unit [i]);
      }
    return SCPE_OK;
  }

t_stat cableFNP (int fnp_unit_num, int iom_unit_num, int chan_num, int dev_code)
  {
    if (fnp_unit_num < 0 || fnp_unit_num >= (int) fnpDev . numunits)
      {
        sim_printf ("cableFNP: fnp_unit_num out of range <%d>\n", fnp_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_fnp [fnp_unit_num] . iom_unit_num != -1)
      {
        sim_printf ("cableFNP: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, dev_code, DEVT_DN355, chan_type_PSI, fnp_unit_num, & fnpDev, & fnp_unit [fnp_unit_num], fnpIOMCmd);
    if (rc)
      return rc;

    cables_from_ioms_to_fnp [fnp_unit_num] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_fnp [fnp_unit_num] . chan_num = chan_num;
    cables_from_ioms_to_fnp [fnp_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }
 
static int fnp_cmd (UNIT * unitp, pcw_t * pcwp, bool * disc)
  {
    int fnp_unit_num = FNP_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_fnp [fnp_unit_num] . iom_unit_num;
//-    struct fnpState * tape_statep = & fnpState [fnp_unit_num];
    word12 stati = 0;
    word6 rcount = 0;
    word12 residue = 0;
    word3 char_pos = 0;
    bool is_read = true;
    bool initiate = false;
    chanStat chanStatus = chanStatNormal;
    * disc = false;

    int chan = pcwp-> chan;

    iomChannelData_ * chan_data = & iomChannelData [iom_unit_num] [chan];
    if (chan_data -> ptp)
      {
        sim_printf ("PTP in fnp; dev_cmd %o\n", pcwp -> dev_cmd);
        sim_err ("PTP in fnp\n");
      }

    switch (pcwp -> dev_cmd)
      {
        default:
          {
            stati = 04501;
            sim_debug (DBG_ERR, & fnpDev,
                       "%s: Unknown command 0%o\n", __func__, pcwp -> dev_cmd);
            break;
          }
      }

    status_service (iom_unit_num, chan, pcwp -> dev_code, stati, rcount, residue, char_pos, is_read, false, initiate, false, chanStatus, iomStatNormal);

    return 0;
  }

/*
 * fnpIOMCmd()
 *
 */

static int fnpIOMCmd (UNIT * unitp, pcw_t * pcwp)
  {
    int fnp_unit_num = FNP_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_fnp [fnp_unit_num] . iom_unit_num;

    // First, execute the command in the PCW, and then walk the 
    // payload channel mbx looking for IDCWs.

// Ignore a CMD 051 in the PCW
    if (pcwp -> dev_cmd == 051)
      return 1;
    bool disc;
//sim_printf ("1 st call to fnp_cmd\n");
    fnp_cmd (unitp, pcwp, & disc);

    // ctrl of the pcw is observed to be 0 even when there are idcws in the
    // list so ignore that and force it to 2.
    //uint ctrl = pcwp -> control;
    uint ctrl = 2;

    int ptro = 0;
//#define PTRO
#ifdef PTRO
    while ((! disc) /* && ctrl == 2 */ && ! ptro)
#else
    while ((! disc) && ctrl == 2)
#endif
      {
        dcw_t dcw;
        int rc = iomListService (iom_unit_num, pcwp -> chan, & dcw, & ptro);
        if (rc)
          {
            break;
          }
        if (dcw . type != idcw)
          {
// 04501 : COMMAND REJECTED, invalid command
            status_service (iom_unit_num, pcwp -> chan, dcw . fields . instr. dev_code, 04501, 0, 0, 0, true, false, false, false, chanStatInvalidInstrPCW, iomStatNormal);
            break;
          }


// The dcw does not necessarily have the same dev_code as the pcw....

        fnp_unit_num = findFNPUnit (iom_unit_num, pcwp -> chan, dcw . fields . instr. dev_code);
        if (fnp_unit_num < 0)
          {
// 04502 : COMMAND REJECTED, invalid device code
            status_service (iom_unit_num, pcwp -> chan, dcw . fields . instr. dev_code, 04502, 0, 0, 0, true, false, false, false, chanStatIncorrectDCW, iomStatNormal);
            break;
          }
        unitp = & fnp_unit [fnp_unit_num];
//sim_printf ("next call to fnp_cmd\n");
        fnp_cmd (unitp, & dcw . fields . instr, & disc);
        ctrl = dcw . fields . instr . control;
//sim_printf ("disc %d ctrl %d\n", disc, ctrl);
      }
    send_terminate_interrupt (iom_unit_num, pcwp -> chan);

    return 1;
  }

static t_stat fnpSVC (UNIT * unitp)
  {
    int fnpUnitNum = FNP_UNIT_NUM (unitp);
    int iomUnitNum = cables_from_ioms_to_fnp [fnpUnitNum] . iom_unit_num;
    int chanNum = cables_from_ioms_to_fnp [fnpUnitNum] . chan_num;
    pcw_t * pcwp = & iomChannelData [iomUnitNum] [chanNum] . pcw;
    fnpIOMCmd (unitp, pcwp);
    return SCPE_OK;
  }
    

#if 0
static int fnp_iom_io (UNIT * unitp, uint chan, uint dev_code, uint * tally, uint * cp, word36 * wordp, word12 * stati)
  {
    //sim_debug (DBG_DEBUG, & fnpDev, "%s\n", __func__);
    int fnp_unit_num = FNP_UNIT_NUM (unitp);
    //int iom_unit_num = cables_from_ioms_to_fnp [fnp_unit_num] . iom_unit_num;
//--     
//--     int dev_unit_num;
//--     DEVICE* devp = get_iom_channel_dev (iom_unit_num, chan, dev_code, & dev_unit_num);
//--     if (devp == NULL || devp->units == NULL) {
//--         *majorp = 05;
//--         *subp = 2;
//--         sim_debug (DBG_ERR, &fnpDev, "fnp_iom_io: Internal error, no device and/or unit for channel 0%o\n", chan);
//--         return 1;
//--     }
//--     UNIT * unitp = & devp -> units [dev_unit_num];
//--     // BUG: no dev_code
//--     
    struct fnpState * tape_statep = & fnpState [fnp_unit_num];
    
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
    sim_printf("Number of TAPE units in system is %d\n", fnpDev . numunits);
    return SCPE_OK;
  }

static t_stat fnpSetNUnits (UNUSED UNIT * uptr, UNUSED int32 value, 
                             char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_FNP_UNITS_MAX)
      return SCPE_ARG;
    fnpDev . numunits = (uint32) n;
    return SCPE_OK;
  }

