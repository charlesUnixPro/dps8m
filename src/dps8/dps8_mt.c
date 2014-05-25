/**
 * \file dps8_mt.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

// 28Nov13 CAC Reworked extr and getbit into extr36; move bytes instead of bits.

#include <stdio.h>
#include "dps8.h"
#include "dps8_mt.h"
#include "dps8_sys.h"
#include "dps8_utils.h"

/*
 mt.c -- mag tape
 See manual AN87
 */
/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

/*
 
 COMMENTS ON "CHAN DATA" AND THE T&D TAPE (test and diagnostic tape)
 
 The IOM status service provides the "residue" from the last PCW or
 IDCW as part of the status.  Bootload_tape_label.alm indicates
 that after a read binary operation, the field is interpreted as the
 device number and that a device number of zero is legal.
 
 The IOM boot channel will store an IDCW with a chan-data field of zero.
 AN70, page 2-1 says that when the tape is read in native mode via an
 IOM or IOCC, the tape drive number in the IDCW will be zero.  It says
 that a non-zero tape drive number in the IDCW indicates that BOS is
 being used to simulate an IOM. (Presumaby written before BCE replaced
 BOS.)
 
 However...
 
 This seems to imply that an MPC could be connected to a single
 channel and support multiple tape drives by using chan-data as a
 device id.  If so, it seems unlikely that chan-data could ever
 represent anything else such as a count of a number of records to
 back space over (which is hinted at as an example in AN87).
 
 Using chan-data as a device-id that is zero from the IOM also
 implies that Multics could only be cold booted from a tape drive with
 device-id zero.  That doesn't seem to mesh with instructions
 elsewhere... And BCE has to (initially) come from the boot tape...

     Comment by CAC:  From MDD-005-02:

       "     bootload_tape_label  is read  in by  one of  two means.   In
        native mode, the  IOM or IMU reads it into  absolute location 30,
        leaving  the PCW,  DCW's, and   other essentials  in locations  0
        through  5.  The IMU  leaves an indication  of its identity  just
        after this block of information.

             In  BOS compatibility mode,  the BOS BOOT  command simulates
        the IOM, leaving the same information.  However, it also leaves a
        config deck and flagbox (although bce has its own flagbox) in the
        usual locations.   This allows Bootload Multics to  return to BOS
        if there is a BOS to return to.  The presence of BOS is indicated
        by the tape drive number being  non-zero in the idcw in the "IOM"
        provided information.   (This is normally zero  until firmware is
        loaded into the bootload tape MPC.) 

 The T&D tape seems to want to see non-zero residue from the very
 first tape read.  That seems to imply that the T&D tape could not
 be booted by the IOM!  Perhaps the T&D tape requires BCE (which
 replaced BOS) ?
 
 TODO
 
 When simulating timing, switch to queuing the activity instead
 of queueing the status return.   That may allow us to remove most
 of our state variables and more easily support save/restore.

 Convert the rest of the routines to have a chan_devinfo argument.
 
 Allow multiple tapes per channel.
 */

#include "sim_tape.h"

#include "dps8_iom.h"
#include "dps8_mt.h"

static t_stat mt_rewind (UNIT * uptr, int32 value, char * cptr, void * desc);
static t_stat mt_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat mt_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static int mt_iom_cmd (UNIT * unitp, pcw_t * p, word12 * stati, bool * need_data, bool * is_read);
static int mt_iom_io (UNIT * unitp, uint chan, uint dev_code, uint * tally, uint * cp, word36 * wordp, word12 * stati);

#define N_MT_UNITS_MAX 16
#define N_MT_UNITS 1 // default

static UNIT mt_unit [N_MT_UNITS_MAX] = {
    // NOTE: other SIMH tape sims don't set UNIT_SEQ
    // CAC: Looking at SIMH source, the only place UNIT_SEQ is used
    // by the "run" command's reset sequence; units that have UNIT_SEQ
    // set will be issued a rewind on reset.
    // Looking at the sim source again... It is used on several of the
    // run commands, including CONTINUE.
    // Turning UNIT_SEQ off.
    // XXX Should we rewind on reset? What is the actual behavior?
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL /*&mt_svc*/, UNIT_ATTABLE | /* UNIT_SEQ | */ UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL}
};

static DEBTAB mt_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };

#define UNIT_WATCH (1 << MTUF_V_UF)

static MTAB mt_mod [] =
  {
    { UNIT_WATCH, UNIT_WATCH, "WATCH", "WATCH", NULL, NULL, NULL, NULL },
    { UNIT_WATCH, 0, "NOWATCH", "NOWATCH", NULL, NULL, NULL, NULL },
    {
       MTAB_XTD | MTAB_VUN | MTAB_NC, /* mask */
      0,            /* match */
      NULL,         /* print string */
      "REWIND",     /* match string */
      mt_rewind,    /* validation routine */
      NULL,         /* display routine */
      NULL,         /* value descriptor */
      NULL          // help
    },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      mt_set_nunits, /* validation routine */
      mt_show_nunits, /* display routine */
      "Number of TAPE units in the system", /* value descriptor */
      NULL          // help
    },
    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
  };

#define MT_UNIT_NUM(uptr) ((uptr) - mt_unit)

static t_stat mt_reset (DEVICE * dptr);

DEVICE tape_dev = {
    "TAPE",           /* name */
    mt_unit,          /* units */
    NULL,             /* registers */
    mt_mod,           /* modifiers */
    N_MT_UNITS,       /* #units */
    10,               /* address radix */
    31,               /* address width */
    1,                /* address increment */
    8,                /* data radix */
    9,                /* data width */
    NULL,             /* examine routine */
    NULL,             /* deposit routine */
    mt_reset,         /* reset routine */
    NULL,             /* boot routine */
    &sim_tape_attach, /* attach routine */
    &sim_tape_detach, /* detach routine */
    NULL,             /* context */
    DEV_DEBUG,        /* flags */
    0,                /* debug control flags */
    mt_dt,            /* debug flag names */
    NULL,             /* memory size change */
    NULL,             /* logical name */
    NULL,             // attach help
    NULL,             // help
    NULL,             // help context
    NULL,             // device description
};

//-- /* unfinished; copied from tape_dev */
static const char * simh_tape_msg (int code); // hack
static const size_t bufsz = 4096 * 1024;
static struct tape_state
  {
    enum { no_mode, read_mode, write_mode } io_mode;
    uint8 * bufp;
    t_mtrlnt tbc; // Number of bytes read into buffer
    uint words_processed; // Number of Word36 processed from the buffer
// XXX bug: 'sim> set tapeN rewind' doesn't reset rec_num
    int rec_num; // track tape position
  } tape_state [N_MT_UNITS_MAX];

static struct
  {
    int iom_unit_num;
    int chan_num;
    int dev_code;
  } cables_from_ioms_to_mt [N_MT_UNITS_MAX];

void mt_init(void)
  {
    memset(tape_state, 0, sizeof(tape_state));
    for (int i = 0; i < N_MT_UNITS_MAX; i ++)
      cables_from_ioms_to_mt [i] . iom_unit_num = -1;
  }

static t_stat mt_reset (DEVICE * dptr)
  {
    for (int i = 0; i < (int) dptr -> numunits; i ++)
      {
        sim_tape_reset (& mt_unit [i]);
        sim_cancel (& mt_unit [i]);
      }
    return SCPE_OK;
  }

//-- int get_mt_numunits (void)
//--   {
//--     return tape_dev . numunits;
//--   }
//-- 
//-- //
//-- // String a cable from a tape drive to an IOM
//-- //
//-- // This end: mt_unit_num
//-- // That end: iom_unit_num, chan_num, dev_code
//-- // 

t_stat cable_mt (int mt_unit_num, int iom_unit_num, int chan_num, int dev_code)
  {
    if (mt_unit_num < 0 || mt_unit_num >= (int) tape_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_mt: mt_unit_num out of range <%d>\n", mt_unit_num);
        sim_printf ("cable_mt: mt_unit_num out of range <%d>\n", mt_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_mt [mt_unit_num] . iom_unit_num != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_mt: socket in use\n");
        sim_printf ("cable_mt: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, dev_code, DEVT_TAPE, chan_type_PSI, mt_unit_num, & tape_dev, & mt_unit [mt_unit_num], mt_iom_cmd, mt_iom_io);
    if (rc)
      return rc;

    cables_from_ioms_to_mt [mt_unit_num] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_mt [mt_unit_num] . chan_num = chan_num;
    cables_from_ioms_to_mt [mt_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

/*
 * mt_iom_cmd()
 *
 */

static int mt_iom_cmd (UNIT * unitp, pcw_t * pcwp, word12 * stati, bool * need_data, bool * is_read)
  {
    * need_data = false;
    int mt_unit_num = MT_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_mt [mt_unit_num] . iom_unit_num;
// XXX
//--     int iom_unit_num = devinfop -> iom_unit_num;
//--     int chan = devinfop->chan;
//--     int dev_cmd = devinfop->dev_cmd;
//--     int dev_code = devinfop->dev_code;
//--     int* majorp = &devinfop->major;
//--     int* subp = &devinfop->substatus;
    

    sim_debug (DBG_DEBUG, &tape_dev, "%s: IOM %c, Chan 0%o, dev-cmd 0%o, dev-code 0%o\n",
            __func__, 'A' + iom_unit_num, pcwp -> chan, pcwp -> dev_cmd, pcwp -> dev_code);

    // XXX do right when write
    * is_read = true;

//--     
//--     devinfop->is_read = 1;
//--     devinfop->time = -1;
//--     
//--     // Major codes are 4 bits...
//--     
//--     if (chan < 0 || chan >= max_channels) {
//--         devinfop->have_status = 1;
//--         *majorp = 05;   // Real HW could not be on bad channel
//--         *subp = 2;
//--         sim_debug (DBG_ERR, &tape_dev, "%s: Bad channel %d\n", __func__, chan);
//--         cancel_run(STOP_BUG);
//--         return 1;
//--     }
//--     
//--     int unit_dev_num;
//--     DEVICE* devp = get_iom_channel_dev (iom_unit_num, chan, dev_code, & unit_dev_num);
//--     if (devp == NULL || devp->units == NULL) {
//--         devinfop->have_status = 1;
//--         *majorp = 05;
//--         *subp = 2;
//--         sim_debug (DBG_ERR, &tape_dev, "%s: Internal error, no device and/or unit for IOM %c channel 0%o\n", __func__, 'A' + iom_unit_num, chan);
//--         cancel_run(STOP_BUG);
//--         return 1;
//--     }
//-- // XXX dev_code != unit_num; this test is incorrect
//-- // XXX it should compare dev_code to the units own idea of it's dev_code
//-- #if 0
//--     if (dev_code < 0 || dev_code >= devp->numunits) {
//--         devinfop->have_status = 1;
//--         *majorp = 05;   // Command Reject
//--         *subp = 2;      // Invalid Device Code
//--         sim_debug (DBG_ERR, &tape_dev, "%s: Bad dev unit-num 0%o (%d decimal)\n", __func__, dev_code, dev_code);
//--         cancel_run(STOP_BUG);
//--         return 1;
//--     }
//-- #endif
//-- 
//--     UNIT* unitp = &devp->units[unit_dev_num];
//--     
    struct tape_state * tape_statep = & tape_state [mt_unit_num];
     
    switch (pcwp -> dev_cmd)
      {
        case 0: // CMD 00 Request status
          {
            * stati = 04000; // have_status = 1
            if (sim_tape_wrp (unitp))
              * stati |= 1;
            if (sim_tape_bot (unitp))
              * stati |= 2;
// XXX This not what sim_tape_eot does
//--             if (sim_tape_eot(unitp)) {
//--                 *majorp = 044;  // BUG? should be 3?
//--                 *subp = 023;    // BUG: should be 040?
//--             }
//--             // todo: switch to having all cmds update status reg?
//--             // This would allow setting 047 bootload complete after
//--             // power-on -- if we need that...
            sim_debug (DBG_INFO, & tape_dev, 
                       "%s: Request status is %06o.\n",
                       __func__, * stati);
            return 0;
          }
        case 5: // CMD 05 -- Read Binary Record
          {
            // We read the record into the tape controllers memory;
            // IOM can subsequently retrieve the data via DCWs.
            if (tape_statep -> bufp == NULL)
              {
                if ((tape_statep -> bufp = malloc (bufsz)) == NULL)
                  {
                    sim_debug (DBG_ERR, & tape_dev,
                               "%s: Malloc error\n", __func__);
                    * stati = 05201; // BUG: arbitrary error code; config switch
                    return 1;
                  }
              }
            t_mtrlnt tbc = 0;
            int ret;
            if (! (unitp -> flags & UNIT_ATT))
              ret = MTSE_UNATT;
            else
              {
                ret = sim_tape_rdrecf (unitp, tape_statep -> bufp, & tbc,
                                       bufsz);
                // XXX put unit number in here...
              }
            if (ret != 0)
              {
                if (ret == MTSE_TMK || ret == MTSE_EOM)
                  {
                    sim_debug (DBG_NOTIFY, & tape_dev,
                                "%s: EOF: %s\n", __func__, simh_tape_msg (ret));
                    * stati = 04423; // EOF category EOF file mark
                    if (tbc != 0)
                      {
                        sim_debug (DBG_ERR, &tape_dev,
                                   "%s: Read %d bytes with EOF.\n", 
                                   __func__, tbc);
                      }
                    return 0;
                  }
                else
                  {
                    sim_debug (DBG_ERR, & tape_dev,
                               "%s: Cannot read tape: %d - %s\n",
                               __func__, ret, simh_tape_msg(ret));
                    sim_debug (DBG_ERR, & tape_dev,
                               "%s: Returning arbitrary error code\n",
                               __func__);
                    * stati = 05001; // BUG: arbitrary error code; config switch
                    return 1;
                  }
              }
            tape_statep -> rec_num ++;
            tape_statep -> tbc = tbc;
            tape_statep -> words_processed = 0;
            if (unitp->flags & UNIT_WATCH)
              sim_printf ("Tape %ld reads record %d\n",
                          MT_UNIT_NUM (unitp), tape_statep -> rec_num);
            * stati = 04000; // have_status = 1
            if (sim_tape_wrp (unitp))
              * stati |= 1;
            tape_statep -> io_mode = read_mode;
            * need_data = true;
            sim_debug (DBG_INFO, & tape_dev,
                       "%s: Read %d bytes from simulated tape\n",
                       __func__, (int) tbc);
            return 0;
          }
//--         case 040:               // CMD 040 -- Reset Status
//--             devinfop->have_status = 1;
//--             *majorp = 0;
//--             *subp = 0;
//--             if (sim_tape_wrp(unitp))
//--               *subp |= 1;
//--             sim_debug (DBG_INFO, &tape_dev, "mt_iom_cmd: Reset status is %02o,%02o.\n",
//--                     *majorp, *subp);
//--             return 0;
//--         case 046: {             // BSR
//--             // BUG: Do we need to clear the buffer?
//--             // BUG? We don't check the channel data for a count
//--             t_mtrlnt tbc;
//--             int ret;
//--             if ((ret = sim_tape_sprecr(unitp, &tbc)) == 0) {
//--                 sim_debug (DBG_NOTIFY, &tape_dev, "mt_iom_cmd: Backspace one record\n");
//--                 // XXX put unit number in here...
//--                 if (unitp->flags & UNIT_WATCH)
//--                   sim_printf ("Tape %ld backspaces over record %d\n",
//--                               MT_UNIT_NUM (unitp), tape_statep -> rec_num);
//-- 
//--                 tape_statep -> rec_num --;
//--                 devinfop->have_status = 1;  // TODO: queue
//--                 *majorp = 0;
//--                 *subp = 0;
//--                 if (sim_tape_wrp(unitp)) *subp |= 1;
//--             } else {
//--                 sim_debug (DBG_ERR, &tape_dev, "mt_iom_cmd: Cannot backspace record: %d - %s\n", ret, simh_tape_msg(ret));
//--                 devinfop->have_status = 1;
//--                 if (ret == MTSE_BOT) {
//--                     *majorp = 05;
//--                     *subp = 010;
//--                 } else {
//--                     sim_debug (DBG_ERR, &tape_dev, "mt_iom_cmd: Returning arbitrary error code\n");
//--                     *majorp = 010;  // BUG: arbitrary error code; config switch
//--                     *subp = 1;
//--                 }
//--                 return 1;
//--             }
//--             return 0;
//--         }

        case 040:               // CMD 040 -- Reset Device Status
            // BUG: How should 040 reset status differ from 051 reset device
            // status?  Presumably the former is for the MPC itself...
            * stati = 04000;
            if (sim_tape_wrp (unitp))
              * stati |= 1;
            sim_debug (DBG_INFO, & tape_dev,
                       "%s: Reset device status is %04o.\n",
                       __func__, * stati);
            return 0;

        default:
          {
            * stati = 04501;
            sim_debug (DBG_ERR, & tape_dev,
                       "%s: Unknown command 0%o\n", __func__, pcwp -> dev_cmd);
            return 1;
        }
      }
    return 1;   // not reached
  }

// Extract the N'th 36 bit word from a buffer
//
//   bits: buffer of bits from a simh tape. The data is
//   packed as 2 36 bit words in 9 eight bit bytes (2 * 36 == 7 * 9)
//   The of the bytes in bits is
//      byte     value
//       0       most significant byte in word 0
//       1       2nd msb in word 0
//       2       3rd msb in word 0
//       3       4th msb in word 0
//       4       upper half is 4 least significant bits in word 0
//               lower half is 4 most significant bit in word 1
//       5       5th to 13th most signicant bits in word 1
//       6       ...
//       7       ...
//       8       least significant byte in word 1
//

// Multics humor: this is idiotic

static t_uint64 extr36 (uint8 * bits, uint woffset)
  {
    uint isOdd = woffset % 2;
    uint dwoffset = woffset / 2;
    uint8 * p = bits + dwoffset * 9;

    t_uint64 w;
    if (isOdd)
      {
        w  = ((t_uint64) (p [4] & 0xf)) << 32;
        w |=  (t_uint64) (p [5]) << 24;
        w |=  (t_uint64) (p [6]) << 16;
        w |=  (t_uint64) (p [7]) << 8;
        w |=  (t_uint64) (p [8]);
      }
    else
      {
        w  =  (t_uint64) (p [0]) << 28;
        w |=  (t_uint64) (p [1]) << 20;
        w |=  (t_uint64) (p [2]) << 12;
        w |=  (t_uint64) (p [3]) << 4;
        w |= ((t_uint64) (p [4]) >> 4) & 0xf;
      }
    // DMASK shouldn't be neccessary but is robust
    return w & DMASK;
  }


static int extractWord36FromBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, t_uint64 *wordp)
  {
    uint wp = * words_processed; // How many words have been processed

    // 2 dps8m words == 9 bytes

    uint bytes_processed = (wp * 9 + 1) / 2;
    if (bytes_processed >= tbc)
      return 1;
    //sim_printf ("store 0%08lo@0%012llo\n", wordp - M, extr36 (bufp, wp));

    * wordp = extr36 (bufp, wp);
    //sim_printf ("* %06lo = %012llo\n", wordp - M, * wordp);
    (* words_processed) ++;

    return 0;
  }

static int mt_iom_io (UNIT * unitp, uint chan, uint __attribute__((unused)) dev_code, uint * tally, uint * __attribute__((unused)) cp, word36 * wordp, word12 * stati)
  {
    //sim_debug (DBG_DEBUG, & tape_dev, "%s\n", __func__);
    int mt_unit_num = MT_UNIT_NUM (unitp);
    //int iom_unit_num = cables_from_ioms_to_mt [mt_unit_num] . iom_unit_num;
//--     
//--     int dev_unit_num;
//--     DEVICE* devp = get_iom_channel_dev (iom_unit_num, chan, dev_code, & dev_unit_num);
//--     if (devp == NULL || devp->units == NULL) {
//--         *majorp = 05;
//--         *subp = 2;
//--         sim_debug (DBG_ERR, &tape_dev, "mt_iom_io: Internal error, no device and/or unit for channel 0%o\n", chan);
//--         return 1;
//--     }
//--     UNIT * unitp = & devp -> units [dev_unit_num];
//--     // BUG: no dev_code
//--     
    struct tape_state * tape_statep = & tape_state [mt_unit_num];
    
    if (tape_statep -> io_mode == no_mode)
      {
        // no prior read or write command
        * stati = 05302; // MPC Device Data Alert Inconsistent command
        sim_debug (DBG_ERR, & tape_dev, "%s: Bad channel %d\n", __func__, chan);
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
                sim_debug (DBG_WARN, & tape_dev,
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
    else
      {
        // write
        sim_debug (DBG_ERR, & tape_dev, "%s: Write I/O Unimplemented\n",
                   __func__);
        * stati = 04340; // Reflective end of tape mark found while trying to write
        return 1;
      }
    
//--     /*notreached*/
//--     *majorp = 0;
//--     *subp = 0;
//--     sim_debug (DBG_ERR, &tape_dev, "mt_iom_io: Internal error.\n");
//--     cancel_run(STOP_BUG);
//    return 1;
  }
//-- 
//-- static t_stat mt_svc(UNIT *up)
//-- {
//--     sim_debug (DBG_DEBUG, &tape_dev, "mt_svc: Calling channel service.\n");
//--     return channel_svc(up);
//-- }

static const char *simh_tape_msg(int code)
{
    // WARNING: Only selected SIMH tape routines return private tape codes
    // WARNING: returns static buf
    // BUG: Is using a string constant equivalent to using a static buffer?
    // static char msg[80];
    if (code == MTSE_OK)
        return "OK";
    else if (code == MTSE_UNATT)
        return "Unit not attached to a file";
    else if (code == MTSE_FMT)
        return "Unit specifies an unsupported tape file format";
    else if (code == MTSE_IOERR)
        return "Host OS I/O error";
    else if (code == MTSE_INVRL)
        return "Invalid record length (exceeds maximum allowed)";
    else if (code == MTSE_RECE)
        return "Record header contains error flag";
    else if (code == MTSE_TMK)
        return "Tape mark encountered";
    else if (code == MTSE_BOT)
        return "BOT encountered during reverse operation";
    else if (code == MTSE_EOM)
        return "End of Medium encountered";
    else if (code == MTSE_WRP)
        return "Write protected unit during write operation";
    else
        return "Unknown SIMH tape error";
  }

static t_stat mt_rewind (UNIT * uptr, int32 __attribute__((unused)) value, char * __attribute__((unused)) cptr, void * __attribute__((unused)) desc)
  {
    return sim_tape_rewind (uptr);
  }

static t_stat mt_show_nunits (FILE * __attribute__((unused)) st, UNIT * __attribute__((unused)) uptr, int __attribute__((unused)) val, void * __attribute__((unused)) desc)
  {
    sim_printf("Number of TAPE units in system is %d\n", tape_dev . numunits);
    return SCPE_OK;
  }

static t_stat mt_set_nunits (UNIT * __attribute__((unused)) uptr, int32 __attribute__((unused)) value, char * cptr, void * __attribute__((unused)) desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_MT_UNITS_MAX)
      return SCPE_ARG;
    tape_dev . numunits = (uint32) n;
    return SCPE_OK;
  }

