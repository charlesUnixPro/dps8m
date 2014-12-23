#include <stdio.h>
#include "dps8.h"
#include "dps8_fnp.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"
#include "fnpp.h"

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
    memset(fnpState, 0, sizeof(fnpState));
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
//-    struct fnpState * tape_statep = & fnpState [fnpUnitNum];
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

    // First, execute the command in the PCW, and then walk the 
    // payload channel mbx looking for IDCWs.

// Ignore a CMD 051 in the PCW
    if (pcwp -> dev_cmd == 051)
      return 1;
sim_printf ("fnpIOMCmd\n");
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
    bool disc;
sim_printf ("1 st call to fnpCmd\n");
    fnpCmd (unitp, pcwp, & disc);

    // ctrl of the pcw is observed to be 0 even when there are idcws in the
    // list so ignore that and force it to 2.
    //uint ctrl = pcwp -> control;
    uint ctrl = 2;

    int ptro = 0;
//#define PTRO
sim_printf ("starting list service loop\n");
#ifdef PTRO
    while ((! disc) /* && ctrl == 2 */ && ! ptro)
#else
    while ((! disc) && ctrl == 2)
#endif
      {
        dcw_t dcw;
sim_printf ("calling list service\n");
        int rc = iomListService (iomUnitNum, pcwp -> chan, & dcw, & ptro);
sim_printf ("list service returned %d %012llo\n", rc, dcw . raw);
        if (rc)
          {
            break;
          }
#if 0
        if (dcw . type != idcw)
          {
sim_printf ("list service not idcw\n");
// 04501 : COMMAND REJECTED, invalid command
            iomChannelData_ * chan_data = & iomChannelData [iomUnitNum] [pcwp -> chan];
            chan_data -> stati = 04501; 
            chan_data -> dev_code = dcw . fields . instr. dev_code;
            chan_data -> chanStatus = chanStatInvalidInstrPCW;
            //status_service (iomUnitNum, pcwp -> chan, false);
            break;
          }
#endif

// The dcw does not necessarily have the same dev_code as the pcw....

        if (dcw . type == idcw)
          {
            fnpUnitNum = findFNPUnit (iomUnitNum, pcwp -> chan, dcw . fields . instr. dev_code);
            if (fnpUnitNum < 0)
              {
sim_printf ("list service invalid device code\n");
                // 04502 : COMMAND REJECTED, invalid device code
                iomChannelData_ * chan_data = & iomChannelData [iomUnitNum] [pcwp -> chan];
                chan_data -> stati = 04502; 
                chan_data -> dev_code = dcw . fields . instr. dev_code;
                chan_data -> chanStatus = chanStatIncorrectDCW;
                //status_service (iomUnitNum, pcwp -> chan, false);
                break;
              }
          }

        unitp = & fnp_unit [fnpUnitNum];
sim_printf ("next call to fnpCmd; instr is %012llo\n", dcw . raw);

        if (dcw . type == idcw)
          {
            fnpCmd (unitp, & dcw . fields . instr, & disc);
          }
        else if (dcw . type == idcw)
          {
            //fnpIOT (unitp, & dcw . fields . ddcw, & disc);
          }
        else
          {
            sim_printf ("fnpIOMCmd dazed and confused\n");
            sim_err ("fnpIOMCmd dazed and confused\n");
          }
        ctrl = dcw . fields . instr . control;
//sim_printf ("disc %d ctrl %d\n", disc, ctrl);
      }
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
    struct fnpState * tape_statep = & fnpState [fnpUnitNum];
    
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
    return fnppSetNunits (uptr, value, cptr, desc);
  }

