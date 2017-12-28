/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2017 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#include <stdio.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_dia.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"

#include "udplib.h"

#define DBG_CTR 1

#ifdef THREADZ
#include "threadz.h"
#endif

static inline void fnp_core_read (word24 addr, word36 *data, UNUSED const char * ctx)
  {
#ifdef THREADZ
    lock_mem ();
#endif
#ifdef SCUMEM
    iom_core_read (addr, data, ctx);
#else
    * data = M [addr] & DMASK;
#endif
#ifdef THREADZ
    unlock_mem ();
#endif
  }
#define N_DIA_UNITS 1 // default
#define DIA_UNIT_IDX(uptr) ((uptr) - dia_unit)

static config_list_t dia_config_list [] =
  {
    /*  0 */ { "mailbox", 0, 07777, NULL },
    { NULL, 0, 0, NULL }
  };

static t_stat set_config (UNIT * uptr, UNUSED int value, const char * cptr, UNUSED void * desc)
  {
    uint dia_unit_idx = (uint) DIA_UNIT_IDX (uptr);
    //if (dia_unit_idx >= dia_dev.numunits)
    if (dia_unit_idx >= N_DIA_UNITS_MAX)
      {
        sim_debug (DBG_ERR, & dia_dev, "DIA SET CONFIG: Invalid unit number %d\n", dia_unit_idx);
        sim_printf ("error: DIA SET CONFIG: invalid unit number %d\n", dia_unit_idx);
        return SCPE_ARG;
      }

    struct dia_unit_data * dudp = dia_data.dia_unit_data + dia_unit_idx;

    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("DIA SET CONFIG", cptr, dia_config_list, & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 

            case -1: // done
              break;

            case 0: // mailbox
              dudp -> mailbox_address = (uint) v;
              break;

            default:
              sim_debug (DBG_ERR, & dia_dev, "DIA SET CONFIG: Invalid cfgparse rc <%d>\n", rc);
              sim_printf ("error: DIA SET CONFIG: invalid cfgparse rc <%d>\n", rc);
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }

static t_stat show_config (UNUSED FILE * st, UNIT * uptr, UNUSED int val, 
                           UNUSED const void * desc)
  {
    long unit_idx = DIA_UNIT_IDX (uptr);
    if (unit_idx >= (long) N_DIA_UNITS_MAX)
      {
        sim_debug (DBG_ERR, & dia_dev, 
                   "DIA SHOW CONFIG: Invalid unit number %ld\n", unit_idx);
        sim_printf ("error: invalid unit number %ld\n", unit_idx);
        return SCPE_ARG;
      }

    sim_printf ("DIA unit number %ld\n", unit_idx);
    struct dia_unit_data * dudp = dia_data.dia_unit_data + unit_idx;

    sim_printf ("DIA Mailbox Address:         %04o(8)\n", dudp -> mailbox_address);
 
    return SCPE_OK;
  }


static t_stat show_status (UNUSED FILE * st, UNIT * uptr, UNUSED int val, 
                             UNUSED const void * desc)
  {
    long dia_unit_idx = DIA_UNIT_IDX (uptr);
    if (dia_unit_idx >= (long) dia_dev.numunits)
      {
        sim_debug (DBG_ERR, & dia_dev, 
                   "DIA SHOW STATUS: Invalid unit number %ld\n", dia_unit_idx);
        sim_printf ("error: invalid unit number %ld\n", dia_unit_idx);
        return SCPE_ARG;
      }

    sim_printf ("DIA unit number %ld\n", dia_unit_idx);
    struct dia_unit_data * dudp = dia_data.dia_unit_data + dia_unit_idx;

    sim_printf ("mailbox_address:              %04o\n", dudp->mailbox_address);
    return SCPE_OK;
  }

static t_stat show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                           UNUSED int val, UNUSED const void * desc)
  {
    sim_printf("Number of DIA units in system is %d\n", dia_dev.numunits);
    return SCPE_OK;
  }

static t_stat set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, 
                             const char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_DIA_UNITS_MAX)
      return SCPE_ARG;
    dia_dev.numunits = (uint32) n;
    return SCPE_OK;
  }

static MTAB dia_mod [] =
  {
    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "CONFIG",     /* print string */
      "CONFIG",         /* match string */
      set_config,         /* validation routine */
      show_config, /* display routine */
      NULL,          /* value descriptor */
      NULL   // help string
    },

    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "STATUS",     /* print string */
      "STATUS",         /* match string */
      NULL,         /* validation routine */
      show_status, /* display routine */
      NULL,          /* value descriptor */
      NULL   // help string
    },

    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      set_nunits, /* validation routine */
      show_nunits, /* display routine */
      "Number of DIA units in the system", /* value descriptor */
      NULL          // help
    },
    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
  };


UNIT dia_unit [N_DIA_UNITS_MAX] = {
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL}
};

static DEBTAB dia_DT [] =
  {
    { "TRACE", DBG_TRACE, NULL },
    { "NOTIFY", DBG_NOTIFY, NULL },
    { "INFO", DBG_INFO, NULL },
    { "ERR", DBG_ERR, NULL },
    { "WARN", DBG_WARN, NULL },
    { "DEBUG", DBG_DEBUG, NULL },
    { "ALL", DBG_ALL, NULL }, // don't move as it messes up DBG message
    { NULL, 0, NULL }
  };


static t_stat reset (UNUSED DEVICE * dptr)
  {
    return SCPE_OK;
  }

static t_stat attach (UNIT * uptr, const char * cptr)
  {
    int unitno = (int) (uptr - dia_unit);

    // ATTACH DNn llll:w.x.y.z:rrrr - connect via UDP to a remote simh host

    t_stat ret;
    char * pfn;

    // If we're already attached, then detach ...
    if ((uptr->flags & UNIT_ATT) != 0)
      detach_unit (uptr);

    // Make a copy of the "file name" argument.  udp_create() actually modifies
    // the string buffer we give it, so we make a copy now so we'll have
    // something to display in the "SHOW DNn ..." command.
    pfn = (char *) calloc (CBUFSIZE, sizeof (char));
    if (pfn == NULL)
      return SCPE_MEM;
    strncpy (pfn, cptr, CBUFSIZE);

    // Create the UDP connection.
    ret = udp_create (cptr, & dia_data.dia_unit_data[unitno].link);
    if (ret != SCPE_OK)
      {
        free (pfn);
        return ret;
      }

    uptr->flags |= UNIT_ATT;
    uptr->filename = pfn;
    return SCPE_OK;
  }

// Detach (connect) ...
static t_stat detach (UNIT * uptr)
  {
    int unitno = (int) (uptr - dia_unit);
    t_stat ret;
    if ((uptr->flags & UNIT_ATT) == 0)
      return SCPE_OK;
    if (dia_data.dia_unit_data[unitno].link == NOLINK)
      return SCPE_OK;

    ret = udp_release (dia_data.dia_unit_data[unitno].link);
    if (ret != SCPE_OK)
      return ret;
    dia_data.dia_unit_data[unitno].link = NOLINK;
    uptr->flags &= ~ (unsigned int) UNIT_ATT;
    free (uptr->filename);
    uptr->filename = NULL;
    return SCPE_OK;
  }



DEVICE dia_dev = {
    "DN",           /* name */
    dia_unit,          /* units */
    NULL,             /* registers */
    dia_mod,           /* modifiers */
    N_DIA_UNITS,       /* #units */
    10,               /* address radix */
    31,               /* address width */
    1,                /* address increment */
    8,                /* data radix */
    9,                /* data width */
    NULL,             /* examine routine */
    NULL,             /* deposit routine */
    reset,         /* reset routine */
    NULL,             /* boot routine */
    attach,           /* attach routine */
    detach,           /* detach routine */
    NULL,             /* context */
    DEV_DEBUG,        /* flags */
    0,                /* debug control flags */
    dia_DT,            /* debug flag names */
    NULL,             /* memory size change */
    NULL,             /* logical name */
    NULL,             // attach help
    NULL,             // help
    NULL,             // help context
    NULL,             // device description
    NULL
};

t_dia_data dia_data;

struct dn355_submailbox
  {
    word36 word1; // dn355_no; is_hsla; la_no; slot_no
    word36 word2; // cmd_data_len; op_code; io_cmd
    word36 command_data [3];
    word36 word6; // data_addr, word_cnt;
    word36 pad3 [2];
  };

struct fnp_submailbox // 28 words
  {
                                                                 // AN85
    word36 word1; // dn355_no; is_hsla; la_no; slot_no    // 0      word0
    word36 word2; // cmd_data_len; op_code; io_cmd        // 1      word1
    word36 mystery [26];                                         // word2...
  };

struct mailbox
  {
    word36 dia_pcw;
    word36 mailbox_requests;
    word36 term_inpt_mpx_wd;
    word36 last_mbx_req_count;
    word36 num_in_use;
    word36 mbx_used_flags;
    word36 crash_data [2];
    struct dn355_submailbox dn355_sub_mbxes [8];
    struct fnp_submailbox fnp_sub_mbxes [4];
  };

#define MAILBOX_WORDS (sizeof (struct mailbox) / sizeof (word36))
#define TERM_INPT_MPX_WD (offsetof (struct mailbox, term_inpt_mpx_wd) / sizeof (word36))

#if 0
#ifdef THREADZ
static inline void l_putbits36_1 (word36 volatile * x, uint p, word1 val)
  {
    const int n = 1;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_1: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    unlock_mem ();
  }

static inline void l_putbits36_3 (word36 volatile * x, uint p, word3 val)
  {
    const int n = 3;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_3: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    unlock_mem ();
  }

static inline void l_putbits36_6 (word36 volatile * x, uint p, word6 val)
  {
    const int n = 6;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_6: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    unlock_mem ();
  }

static inline void l_putbits36_9 (word36 volatile * x, uint p, word9 val)
  {
    const int n = 9;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_9: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    unlock_mem ();
  }

static inline void l_putbits36_12 (word36 volatile * x, uint p, word12 val)
  {
    const int n = 12;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_12: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    unlock_mem ();
  }

static inline void l_putbits36_18 (word36 volatile * x, uint p, word18 val)
  {
    const int n = 18;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_18: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    unlock_mem ();
}

#else
#define l_putbits36_1 putbits36_1
#define l_putbits36_3 putbits36_3
#define l_putbits36_6 putbits36_6
#define l_putbits36_9 putbits36_9
#define l_putbits36_12 putbits36_12
#define l_putbits36_18 putbits36_18
#endif
#endif


#if 0
static void setTIMW (uint mailbox_address, int mbx)
  {
    uint timwAddress = mailbox_address + TERM_INPT_MPX_WD;
    l_putbits36_1 (& M [timwAddress], (uint) mbx, 1);
  }
#endif


//
// Once-only initialization
//

void dia_init (void)
  {
    // 0 sets set service to service_undefined
    memset(& dia_data, 0, sizeof(dia_data));
    for (int i = 0; i < N_DIA_UNITS_MAX; i ++)
      {
        cables -> cables_from_iom_to_dia [i].iomUnitIdx = -1;
      }
  }

static inline void fnp_core_write (word24 addr, word36 data, UNUSED const char * ctx)
  {
#ifdef THREADZ
    lock_mem ();
#endif
#ifdef SCUMEM
    iom_core_write (addr, data, ctx);
#else
    M [addr] = data & DMASK;
#endif
#ifdef THREADZ
    unlock_mem ();
#endif
  }

//
// Convert virtual address to physical
//

static uint virtToPhys (uint ptPtr, uint l66Address)
  {
    uint pageTable = ptPtr * 64u;
    uint l66AddressPage = l66Address / 1024u;

    word36 ptw;
#ifdef SCUMEM
    uint iomUnitIdx = (uint) cables->cablesFromIomToFnp [decoded.devUnitIdx].iomUnitIdx;
    iom_core_read (iomUnitIdx, pageTable + l66AddressPage, & ptw, "fnpIOMCmd get ptw");
#else
    fnp_core_read (pageTable + l66AddressPage, & ptw, "fnpIOMCmd get ptw");
#endif
    uint page = getbits36_14 (ptw, 4);
    uint addr = page * 1024u + l66Address % 1024u;
    return addr;
  }

#if 0
//
// Locate an available fnp_submailbox
//

static int findMbx (uint dia_unit_idx)
  {
    struct dia_unit_data * dudp = & dia_data.dia_unit_data [dia_unit_idx];
    for (uint i = 0; i < 4; i ++)
      if (! dudp -> fnpMBXinUse [i])
        return (int) i;
    return -1;
  }
#endif

#if 0
static void notifyCS (int mbx, int fnpno, int lineno)
  {
#ifdef DIADBG
sim_printf ("notifyCS mbx %d\n", mbx);
#endif
    struct dia_unit_data * dudp = & dia_data.dia_unit_data [fnpno];
#ifdef SCUMEM
    uint iom_unit_idx = (uint) cables->cables_from_iom_to_dia [fnpno].iom_unit_idx;
    word24 offset;
    int scu_unit_num =  queryIomScbankMap (iom_unit_idx, dudp->mailbox_address, & offset);
    int scu_unit_idx = cables->cablesFromScus[iom_unit_idx][scu_unit_num].scu_unit_idx;
    struct mailbox vol * mbxp = (struct mailbox vol *) & scu [scu_unit_idx].M[offset];
#else
    struct mailbox vol * mbxp = (struct mailbox vol *) & M [dudp->mailbox_address];
#endif
    struct fnp_submailbox vol * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    l_putbits36_3 (& smbxp -> word1, 0, (word3) fnpno); // dn355_no XXX
    l_putbits36_1 (& smbxp -> word1, 8, 1); // is_hsla XXX
    l_putbits36_3 (& smbxp -> word1, 9, 0); // la_no XXX
    l_putbits36_6 (& smbxp -> word1, 12, (word6) lineno); // slot_no XXX
    l_putbits36_18 (& smbxp -> word1, 18, 256); // blocks available XXX

    dudp->fnpMBXinUse [mbx] = true;
    setTIMW (dudp->mailbox_address, mbx + 8);
    send_terminate_interrupt ((uint) cables -> cables_from_iom_to_dia [fnpno] . iom_unit_idx, (uint) cables -> cables_from_iom_to_dia [fnpno] . chan_num);
  }
#endif

//
// udp packets
//
//   pkt[0] = cmd
//     cmd 1 - bootload
//

static void cmd_bootload (uint iom_unit_idx, uint dev_unit_idx, uint chan, word24 l66_addr)
  {
    
    uint fnpno = dev_unit_idx; // XXX
    //iomChanData_t * p = & iomChanData [iom_unit_idx] [chan];
    struct dia_unit_data * dudp = & dia_data.dia_unit_data[fnpno];
#ifdef SCUMEM
    word24 offset;
    int scu_unit_num =  queryIomScbankMap (iom_unit_idx, dudp->mailbox_address, & offset);
    int scu_unit_idx = cables->cablesFromScus[iom_unit_idx][scu_unit_num].scu_unit_idx;
    struct mailbox vol * mbxp = (struct mailbox *) & scu[scu_unit_idx].M[decoded.dudp->mailbox_address]; 
#else
    struct mailbox vol * mbxp = (struct mailbox vol *) & M[dudp->mailbox_address];
#endif

    dia_data.dia_unit_data[dev_unit_idx].l66_addr = l66_addr;

    dn_bootload pkt;
    pkt.cmd = dn_cmd_bootload;
    //pkt.dia_pcw = mbxp->dia_pcw;

    //sim_printf ("XXXXXXXXXXXXXXXXXXXXXXXXXXX cmd_bootload\r\n");
    int rc = dn_udp_send (dia_data.dia_unit_data[dev_unit_idx].link,
                          (uint8_t *) & pkt,
                          (uint16_t) sizeof (pkt), PFLG_FINAL);
    if (rc < 0)
      {
        printf ("udp_send failed\n");
      }
  }

static int interruptL66 (uint iom_unit_idx, uint chan)
  {
    iomChanData_t * p = & iomChanData[iom_unit_idx][chan];
    struct device * d = & cables->cablesFromIomToDev[iom_unit_idx].
      devices[chan][p->IDCW_DEV_CODE];
    uint dev_unit_idx = d->devUnitIdx;
    struct dia_unit_data * dudp = &dia_data.dia_unit_data[dev_unit_idx];
#ifdef SCUMEM
    word24 offset;
    int scu_unit_num =  queryIomScbankMap (iom_unit_idx, dudp->mailbox_address, & offset);
    int scu_unit_idx = cables->cablesFromScus[iom_unit_idx][scu_unit_num].scu_unit_idx;
    struct mailbox vol * mbxp = (struct mailbox *) & scu[scu_unit_idx].M[dudp->mailbox_address];
#else
    struct mailbox vol * mbxp = (struct mailbox vol *) & M[dudp->mailbox_address];
#endif
    word36 dia_pcw = mbxp -> dia_pcw;

// AN85, pg 13-5
// When the CS has control information or output data to send
// to the FNP, it fills in a submailbox as described in Section 4
// and sends an interrupt over the DIA. This interrupt is handled 
// by dail as described above; when the submailbox is read, the
// transaction control word is set to "submailbox read" so that when
// the I/O completes and dtrans runs, the mailbox decoder (decmbx)
// is called. the I/O command in the submail box is either WCD (for
// control information) or WTX (for output data). If it is WCD,
// decmbx dispatches according to a table of operation codes and
// setting a flag in the IB and calling itest, the "test-state"
// entry of the interpreter. n a few cases, the operation requires
// further DIA I/O, but usually all that remains to be does is to
// "free" the submailbox by turning on the corresponding bit in the
// mailbox terminate interrupt multiplex word (see Section 4) and
// set the transaction control word accordingly. When the I/O to
// update TIMW terminates, the transaction is complete.
//
// If the I/O command is WTX, the submailbox contains the
// address and length of a 'pseudo-DCW" list containing the
// addresses and tallies of data buffers in tty_buf. In this case,
// dia_man connects to a DCW list to read them into a reserved area
// in dia_man. ...


// interrupt level (in "cell"):
//
// mbxs 0-7 are CS -> FNP
// mbxs 8--11 are FNP -> CS
//
//   0-7 Multics has placed a message for the FNP in mbx 0-7.
//   8-11 Multics has updated mbx 8-11
//   12-15 Multics is done with mbx 8-11  (n - 4).

    word6 cell = getbits36_6 (dia_pcw, 24);
#ifdef FNPDBG
sim_printf ("CS interrupt %u\n", cell);
#endif
    if (cell < 8)
      {
        //interruptL66_CS_to_FNP ();
      }
    else if (cell >= 8 && cell <= 11)
      {
        //interruptL66_FNP_to_CS ();
      }
    else if (cell >= 12 && cell <= 15)
      {
        //interruptL66_CS_done ();
      }
    else
      {
        sim_debug (DBG_ERR, & dia_dev, "fnp illegal cell number %d\n", cell);
        sim_printf ("fnp illegal cell number %d\n", cell);
        // doFNPfault (...) // XXX
        return -1;
      }
    return 0;
  }

static void processMBX (uint iom_unit_idx, uint chan)
  {
    iomChanData_t * p = & iomChanData[iom_unit_idx][chan];
    struct device * d = & cables->cablesFromIomToDev[iom_unit_idx].
      devices[chan][p->IDCW_DEV_CODE];
    uint dev_unit_idx = d->devUnitIdx;
    struct dia_unit_data * dudp = &dia_data.dia_unit_data[dev_unit_idx];

// 60132445 FEP Coupler EPS
// 2.2.1 Control Intercommunication
//
// "In Level 66 momory, at a location known to the coupler and
// to Level 6 software is a mailbox area consisting to an Overhead
// mailbox and 7 Channel mailboxes."

    bool ok = true;
    struct mailbox vol * mbxp = (struct mailbox vol *) & M [dudp -> mailbox_address];

    word36 dia_pcw;
    dia_pcw = mbxp -> dia_pcw;
//sim_printf ("mbx %08o:%012"PRIo64"\n", dudp -> mailbox_address, dia_pcw);

// Mailbox word 0:
//
//   0-17 A
//     18 I
//  19-20 MBZ
//  21-22 RFU
//     23 0
//  24-26 B
//  27-29 D Channel #
//  30-35 C Command
//
//                          A6-A23    A0-A2     A3-A5
// Operation          C         A        B        D
// Interrupt L6      071       ---      Int.     Level
// Bootload L6       072    L66 Addr  L66 Addr  L66 Addr
//                           A6-A23    A0-A2     A3-A5
// Interrupt L66     073      ---      ---     Intr Cell
// Data Xfer to L66  075    L66 Addr  L66 Addr  L66 Addr
//                           A6-A23    A0-A2     A3-A5
// Data Xfer to L6   076    L66 Addr  L66 Addr  L66 Addr
//                           A6-A23    A0-A2     A3-A5

// 
// fnp_util.pl1:
//    075 tandd read
//    076 tandd write

// mbx word 1: mailbox_requests fixed bin
//          2: term_inpt_mpx_wd bit (36) aligned
//          3: last_mbx_req_count fixed bin
//          4: num_in_use fixed bin
//          5: mbx_used_flags
//                used (0:7) bit (1) unaligned
//                pad2 bit (28) unaligned
//          6,7: crash_data
//                fault_code fixed bin (18) unal unsigned
//                ic fixed bin (18) unal unsigned
//                iom_fault_status fixed bin (18) unal unsigned
//                fault_word fixed bin (18) unal unsigned
//
//    crash_data according to dn355_boot_interrupt.pl1:
//
//   dcl  1 fnp_boot_status aligned based (stat_ptr),            /* structure of bootload status */
//          2 real_status bit (1) unaligned,                     /* must be "1"b in valid status */
//          2 pad1 bit (2) unaligned,
//          2 major_status bit (3) unaligned,
//          2 pad2 bit (3) unaligned,
//          2 substatus fixed bin (8) unal,                      /* code set by 355, only interesting if major_status is 4 */
//          2 channel_no fixed bin (17) unaligned;               /* channel no. of LSLA in case of config error */
//    only 34 bits???
// major_status:
//  dcl  BOOTLOAD_OK fixed bin int static options (constant) init (0);
//  dcl  CHECKSUM_ERROR fixed bin int static options (constant) init (1);
//  dcl  READ_ERROR fixed bin int static options (constant) init (2);
//  dcl  GICB_ERROR fixed bin int static options (constant) init (3);
//  dcl  INIT_ERROR fixed bin int static options (constant) init (4);
//  dcl  UNWIRE_STATUS fixed bin int static options (constant) init (5);
//  dcl  MAX_STATUS fixed bin int static options (constant) init (5);
 

// 3.5.1 Commands Issued by Central System
//
// In the issuing of an order by the Central System to the Coupler, the 
// sequence occurs:
//
// 1. The L66 program creates a LPW and Pcw for the Central System Connect
// channel. It also generates and stores a control word containing a command
// int he L66 maillbox. A Connect is then issued to the L66 IOM.
//
// 2. The Connect Channel accesses the PCW to get the channel number of
// the Direct Channel that the coupler is attached to. the direct Channel
// sends a signelto the Coupler that a Connect has been issued.
//
// 3. The Coupler now reads the content of the L66 mailbox, obtaining the
// control word. If the control word is legel, the Coupler will write a
// word of all zeros into the mailbox.
//

// 4.1.1.2 Transfer Control Word.
// The transfer control word, which is pointed to by the 
// mailbox word in l66 memory on Op Codes 72, 7, 76 contains
// a starting address which applies to L6 memory an a Tally
// of the number of 36 bit words to be transfered. The l66
// memory locations to/from which the transfers occur are
// those immediately follwoing the location where this word
// was obtained.
//
//    00-02  001
//    03-17 L6 Address
//       18 P
//    19-23 MBZ
//    24-25 Tally
//
//     if P = 0 the l6 address:
//        00-07 00000000
//        08-22 L6 address (bits 3-17)
//           23 0
//     if P = 1
//        00-14 L6 address (bits 3-17)
//        15-23 0
//

    //uint chanNum = getbits36_6 (dia_pcw, 24);
    uint command = getbits36_6 (dia_pcw, 30);
    word36 bootloadStatus = 0;

    if (command == 000) // reset
      {
#ifdef FNPDBG
sim_printf ("reset??\n");
#endif
      }
    else if (command == 072) // bootload
      {
        // 60132445 pg 49
        // Extract L66 address from dia_pcw

// According to 60132445:
        //word24 A = (word24) getbits36_18 (dia_pcw,  0);
        //word24 B = (word24) getbits36_3  (dia_pcw, 24);
        //word24 D = (word24) getbits36_3  (dia_pcw, 29);
        //word24 l66_addr = (B << (24 - 3)) | (D << (24 - 3 - 3)) | A;
// According to fnp_util.pl1:
//     dcl  1 a_dia_pcw aligned based (mbxp),    /* better declaration than the one used when MCS is running */
//            2 address fixed bin (18) unsigned unaligned,
        word24 l66_addr = (word24) getbits36_18 (dia_pcw,  0);
sim_printf ("l66_addr %08o\r\n", l66_addr);

        uint phys_addr = virtToPhys (p->PCW_PAGE_TABLE_PTR, l66_addr);
sim_printf ("phys_addr %08o\r\n", phys_addr);

        word36 tcw;
        fnp_core_read (phys_addr, & tcw, "tcw fetch");

// Got 100000000517 as expected
//sim_printf ("tcw %012llo\r\n", tcw);

        //word36 tcw1;
        //fnp_core_read (phys_addr + 1, & tcw1, "tcw fetch");

// Got 100002060002, as expected (first word of gicb)
//sim_printf ("tcw1 %012llo\r\n", tcw1);

        // pg 50 4.1.1.2 Transfer control word
        // "The transfer control word, which is pointed to by the
        // mailbox word in L66 memory on Op Codes 72, 75, 76 contains
        // a starting address which applies to L6 memory and a Tally
        // of the number of 36 bit words to be transferred. The L66
        // memory locations to/from which the transfers occur are 
        // those immediately following the location where this word
        // was obtained.
        //
        // 0-2: 001
        // 3-17: L6 Address
        // 18: P
        // 19-23: MBZ
        // 24-36: Tally
        //
        // The L6 Address field is interpreted as an effective L6 byte
        // address as follows:
        //
        // If P = 0
        //
        // 0-7: 00000000
        // 8-22: L6 Address field (bits 3-17)
        // 23: 0
        //
        // If P = 1
        //
        // 0-14: L6 Address field (bits 3-17)
        // 15-22: 0000000
        // 23: 0

        cmd_bootload (iom_unit_idx, dev_unit_idx, chan, l66_addr);

        // Don't acknowledge the boot yet.
        //dudp -> fnpIsRunning = true;
        return;
      }
    else if (command == 071) // interrupt L6
      {
        ok = interruptL66 (iom_unit_idx, chan) == 0;
      }
    else if (command == 075) // data xfer from L6 to L66
      {
        // Build the L66 address from the PCW
        //   0-17 A
        //  24-26 B
        //  27-29 D Channel #
        // Operation          C         A        B        D
        // Data Xfer to L66  075    L66 Addr  L66 Addr  L66 Addr
        //                           A6-A23    A0-A2     A3-A5
        // These don't seem to be right; M[L66Add] is always 0.
        //word24 A = (word24) getbits36_18 (dia_pcw,  0);
        //word24 B = (word24) getbits36_3  (dia_pcw, 24);
        //word24 D = (word24) getbits36_3  (dia_pcw, 29);
        //word24 L66Addr = (B << (24 - 3)) | (D << (24 - 3 - 3)) | A;


        // According to fnp_util:
        //  dcl  1 a_dia_pcw aligned based (mbxp),                      /* better declaration than the one used when MCS is running */
        //         2 address fixed bin (18) unsigned unaligned,
        //         2 error bit (1) unaligned,
        //         2 pad1 bit (3) unaligned,
        //         2 parity bit (1) unaligned,
        //         2 pad2 bit (1) unaligned,
        //         2 pad3 bit (3) unaligned,                            /* if we used address extension this would be important */
        //         2 interrupt_level fixed bin (3) unsigned unaligned,
        //         2 command bit (6) unaligned;
        //
        //   a_dia_pcw.address = address;
        //


        //word24 L66Addr = (word24) getbits36_18 (dia_pcw, 0);
        //sim_printf ("L66 xfer\n");
        //sim_printf ("PCW  %012"PRIo64"\n", dia_pcw);
        //sim_printf ("L66Addr %08o\n", L66Addr);
        //sim_printf ("M[] %012"PRIo64"\n", M[L66Addr]);

        // 'dump_mpx d'
        //L66 xfer
        //PCW  022002000075
        //L66Addr 00022002
        //M[] 000000401775
        //L66 xfer
        //PCW  022002000075
        //L66Addr 00022002
        //M[] 003772401775
        //L66 xfer
        //PCW  022002000075
        //L66Addr 00022002
        //M[] 007764401775
        //
        // The contents of M seem much more reasonable, bit still don't match
        // fnp_util$setup_dump_ctl_word. The left octet should be '1', not '0';
        // bit 18 should be 0 not 1. But the offsets and tallies match exactly.
        // Huh... Looking at 'dump_6670_control' control instead, it matches 
        // correctly. Apparently fnp_util thinks the FNP is a 6670, not a 335.
        // I can't decipher the call path, so I don't know why; but looking at
        // multiplexer_types.incl.pl1, I would guess that by MR12.x, all FNPs 
        // were 6670s.
        //
        // So:
        //
        //   dcl  1 dump_6670_control aligned based (data_ptr),          /* word used to supply DIA address and tally for fdump */
        //          2 fnp_address fixed bin (18) unsigned unaligned,
        //          2 unpaged bit (1) unaligned,
        //          2 mbz bit (5) unaligned,
        //          2 tally fixed bin (12) unsigned unaligned;

        // Since the data is marked 'paged', and I don't understand the
        // the paging mechanism or parameters, I'm going to punt here and
        // not actually transfer any data.
#ifdef FNPDBG
sim_printf ("data xfer??\n");
#endif

      }
    else
      {
        sim_warn ("bogus fnp command %d (%o)\n", command, command);
        ok = false;
      }

    if (ok)
      {
#ifdef FNPDBG
//dmpmbx (dudp->mailbox_address);
#endif
        fnp_core_write (dudp -> mailbox_address, 0, "dia_iom_cmd clear dia_pcw");
        putbits36_1 (& bootloadStatus, 0, 1); // real_status = 1
        putbits36_3 (& bootloadStatus, 3, 0); // major_status = BOOTLOAD_OK;
        putbits36_8 (& bootloadStatus, 9, 0); // substatus = BOOTLOAD_OK;
        putbits36_17 (& bootloadStatus, 17, 0); // channel_no = 0;
        fnp_core_write (dudp -> mailbox_address + 6, bootloadStatus, "dia_iom_cmd set bootload status");
      }
    else
      {
        //dmpmbx (dudp->mailbox_address);
        sim_printf ("%s not ok\r\n", __func__);
// 3 error bit (1) unaligned, /* set to "1"b if error on connect */
        putbits36_1 (& dia_pcw, 18, 1); // set bit 18
        fnp_core_write (dudp -> mailbox_address, dia_pcw, "dia_iom_cmd set error bit");
      }
  }

static int dia_cmd (uint iom_unit_idx, uint chan)
  {
    iomChanData_t * p = & iomChanData[iom_unit_idx][chan];
    p -> stati = 0;
//sim_printf ("fnp cmd %d\n", p -> IDCW_DEV_CMD);
    switch (p -> IDCW_DEV_CMD)
      {
        case 000: // CMD 00 Request status
          {
//sim_printf ("fnp cmd request status\n");
#if 0
            if (findPeer ("fnp-d"))
              p -> stati = 04000;
            else
              p -> stati = 06000; // Have status; power off?
#else
              p -> stati = 04000;
#endif
            //disk_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & dia_dev, "Request status\n");
          }
          break;

        default:
          {
            p -> stati = 04501;
            sim_debug (DBG_ERR, & dia_dev,
                       "%s: Unknown command 0%o\n", __func__, p -> IDCW_DEV_CMD);
#ifdef FNPDBG
sim_printf ("%s: Unknown command 0%o\n", __func__, p -> IDCW_DEV_CMD);
#endif
            break;
          }
      }

    processMBX (iom_unit_idx, chan);

    return 2; // did command, don't want more
  }

/*
 * dia_iom_cmd()
 *
 */

// 1 ignored command
// 0 ok
// -1 problem

int dia_iom_cmd (uint iom_unit_idx, uint chan)
  {
sim_printf ("dia_iom_cmd %u %u\r\n", iom_unit_idx, chan);
    iomChanData_t * p = & iomChanData[iom_unit_idx][chan];
// Is it an IDCW?

    if (p -> DCW_18_20_CP == 7)
      {
        return dia_cmd (iom_unit_idx, chan);
      }
    // else // DDCW/TDCW
    sim_printf ("%s expected IDCW\n", __func__);
    return -1;
  }

static void load_stored_boot (void)
  {
    sim_printf ("got load_stored_boot\n");
  }

#define psz 17000
static uint8_t pkt[psz];

// warning: returns ptr to static buffer
static int poll_coupler (uint unitno, uint8_t * * pktp)
  {
    int sz = dn_udp_receive (dia_data.dia_unit_data[unitno].link, pkt, psz);
    if (sz < 0)
      {
        sim_printf ("dn_udp_receive failed: %d\n", sz);
        sz = 0;
      }
    * pktp = pkt;
    return sz;
  }


void dia_unit_process_events (uint unit_num)
  {
// XXX rememeber
// XXX        //dudp -> fnpIsRunning = true;
// XXX when bootload complete!

    uint8_t * pktp;
    int sz = poll_coupler (unit_num, & pktp);
//sim_printf ("poll_coupler return %d\n", sz);
    if (! sz)
      {
        return;
      }

   uint8_t cmd = pktp [0];
   switch (cmd)
     {
       // IO Load Stored Boot
       case dn_cmd_ISB_IOLD:
         {
           load_stored_boot ();
           break;
         }

       default:
         {
           sim_printf ("%s got unhandled cmd %u\n", __func__, cmd);
           break;
         }
     }
  }

void dia_process_events (void)
  {
    for (uint unit_num = 0; unit_num < N_DIA_UNITS_MAX; unit_num ++)
      {
         dia_unit_process_events (unit_num);
      }
  }
