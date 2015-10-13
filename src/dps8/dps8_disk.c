//
//  dps8_disk.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

// source/library_dir_dir/system_library_1/source/bound_volume_rldr_ut_.s.archive/rdisk_.pl1

#include <stdio.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_disk.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_cable.h"
#include "sim_disk.h"

/*
 disk.c -- disk drives
 
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */


// assuming 512 word36  sectors; and seekPosition is seek512
#define SECTOR_SZ_IN_W36 512
#define SECTOR_SZ_IN_BYTES ((36 * SECTOR_SZ_IN_W36) / 8)

#define M3381_SECTORS 6895616
// records per subdev: 74930 (127 * 590)
// number of sub-volumes: 3
// records per dev: 3 * 74930 = 224790
// cyl/sv: 590
// cyl: 1770 (3*590)
// rec/cyl 127
// tracks/cyl 15
// sector size: 512
// sectors: 451858
// data: 3367 MB, 3447808 KB, 6895616 sectors,
//  3530555392 bytes, 98070983 records?

#define N_DISK_UNITS 2 // default

//-- // extern t_stat disk_svc(UNIT *up);

// ./library_dir_dir/include/fs_dev_types.incl.alm
//
// From IBM GA27-1661-3_IBM_3880_Storage_Control_Description_May80, pg 4.4:
//
//  The Seek command transfers the six-byte seek address from the channel to
//  the storage director....
//
//     Bytes 0-5: 0 0 C C 0 H
//
//       Model       Cmax   Hmax
//       3330-1       410     18
//       3330-11      814     18
//       3340 (35MB)  348     11
//       3340 (70MB)  697     11
//       3344         697     11
//       3350         559     29
//
//  Search Identifier Equal  [CC HH R]
//    

// ./library_dir_dir/system_library_1/source/bound_page_control.s.archive/disk_control.pl1
//
// dcl     devadd             fixed bin (18);              /* record number part of device address */
//
//  /* Compute physical sector address from input info.  Physical sector result
//   accounts for unused sectors per cylinder. */
//
//        if pvte.is_sv then do;                  /* convert the subvolume devadd to the real devadd */
//             record_offset = mod (devadd, pvte.records_per_cyl);
//             devadd = ((devadd - record_offset) * pvte.num_of_svs) + pvte.record_factor + record_offset;
//        end;
//        sector = devadd * sect_per_rec (pvte.device_type);/* raw sector. */
//        cylinder = divide (sector, pvtdi.usable_sect_per_cyl, 12, 0);
//        sector = sector + cylinder * pvtdi.unused_sect_per_cyl;
//        sector = sector + sect_off;                     /* sector offset, if any. */
//

// DB37rs DSS190 Disk Subsystem, pg. 27:
//
//   Seek instruction:
//     Sector count limit, bits 0-11: These bits define the binary sector count.
//     All zeros is a maximum count of 4096.
//   Track indicator, bits 12-13:
//     These bits inidicate a complete track as good, defective, or alternate.
//       00 = primary track - good
//       01 = alternate track - good
//       10 = defective track - alternate track assigned
//       11 = defective track - no alternate track assigned
//
//   Sector address, bits 16-35
//
//      0                                                  35
//      XXXX  XXXX | XXXX XXXX | XXXX XXXX | XXXX XXXX | XXXX 0000 
//        BYTE 0         1           2           3           4
//
//  Seek        011100
//  Read        010101
//  Read ASCII  010011
//  Write       011001
//  Write ASCII 011010
//  Write and compare
//              011011
//  Request status
//              000000
//  reset status
//              100000
//  bootload control store
//              001000
//  itr boot    001001
//
static t_stat disk_reset (DEVICE * dptr);
static t_stat disk_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat disk_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
//static int disk_iom_io (UNIT * unitp, uint chan, uint dev_code, uint * tally, uint * cp, word36 * wordp, word12 * stati);

static t_stat disk_svc (UNIT *);

#define UNIT_FLAGS ( UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | \
                     UNIT_IDLE | DKUF_F_RAW)
UNIT disk_unit [N_DISK_UNITS_MAX] =
  {
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& disk_svc, UNIT_FLAGS, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL}
  };

#define DISK_UNIT_NUM(uptr) ((uptr) - disk_unit)

static DEBTAB disk_dt [] =
  {
    { "TRACE", DBG_TRACE },
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };

#define UNIT_WATCH UNIT_V_UF

static MTAB disk_mod [] =
  {
    { UNIT_WATCH, 1, "WATCH", "WATCH", 0, 0, NULL, NULL },
    { UNIT_WATCH, 0, "NOWATCH", "NOWATCH", 0, 0, NULL, NULL },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      disk_set_nunits, /* validation routine */
      disk_show_nunits, /* display routine */
      "Number of DISK units in the system", /* value descriptor */
      NULL // Help
    },
    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
  };


// No disks known to multics had more than 2^24 sectors...
DEVICE disk_dev = {
    "DISK",       /*  name */
    disk_unit,    /* units */
    NULL,         /* registers */
    disk_mod,     /* modifiers */
    N_DISK_UNITS, /* #units */
    10,           /* address radix */
    24,           /* address width */
    1,            /* address increment */
    8,            /* data radix */
    36,           /* data width */
    NULL,         /* examine */
    NULL,         /* deposit */ 
    disk_reset,   /* reset */
    NULL,         /* boot */
    NULL,         /* attach */
    NULL,         /* detach */
    NULL,         /* context */
    DEV_DEBUG,    /* flags */
    0,            /* debug control flags */
    disk_dt,      /* debug flag names */
    NULL,         /* memory size change */
    NULL,         /* logical name */
    NULL,         // help
    NULL,         // attach help
    NULL,         // attach context
    NULL          // description
};

static struct disk_state
  {
    enum { no_mode, seek512_mode, seek_mode, read_mode, write_mode, request_status_mode } io_mode;
    uint seekPosition;
    int chan;
    uint mask;
    uint ext;
  } disk_states [N_DISK_UNITS_MAX];

static int findDiskUnit (int iomUnitIdx, int chan_num, int dev_code)
  {
    for (int i = 0; i < N_DISK_UNITS_MAX; i ++)
      {
        if (iomUnitIdx == cables -> cablesFromIomToDsk [i] . iomUnitIdx &&
            chan_num     == cables -> cablesFromIomToDsk [i] . chan_num     &&
            dev_code     == cables -> cablesFromIomToDsk [i] . dev_code)
          return i;
      }
    return -1;
  }

/*
 * disk_init()
 *
 */

// Once-only initialization

void disk_init (void)
  {
    memset (disk_states, 0, sizeof (disk_states));
  }

static t_stat disk_reset (DEVICE * dptr)
  {
    for (uint i = 0; i < dptr -> numunits; i ++)
      {
        // sim_disk_reset (& disk_unit [i]);
        sim_cancel (& disk_unit [i]);
      }
    return SCPE_OK;
  }

static int diskSeek512 (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
                      devices [chan] [p -> IDCW_DEV_CODE];
    uint devUnitIdx = d -> devUnitIdx;
    UNIT * unitp = & disk_unit [devUnitIdx];
    struct disk_state * disk_statep = & disk_states [devUnitIdx];
    sim_debug (DBG_NOTIFY, & disk_dev, "Seek512 %d\n", devUnitIdx);
//sim_printf ("disk seek512 [%lld]\n", sim_timell ());
    disk_statep -> io_mode = seek512_mode;
    p -> initiate = true;

// Process DDCW

    bool ptro;
    bool send;
    bool uff;
    int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
    if (rc < 0)
      {
        sim_printf ("diskSeek512 list service failed\n");
        return -1;
      }
    if (uff)
      {
        sim_printf ("diskSeek512 ignoring uff\n"); // XXX
      }
    if (! send)
      {
        sim_printf ("diskSeek512 nothing to send\n");
        return 1;
      }
    if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
      {
        sim_printf ("diskSeek512 expected DDCW\n");
        return -1;
      }


    uint tally = p -> DDCW_TALLY;
    uint daddr = p -> DDCW_ADDR;
    if (disk_statep -> mask)
      daddr |= ((disk_statep -> ext) & MASK6) << 18;
    if (tally == 0)
      {
        sim_debug (DBG_DEBUG, & disk_dev,
                   "%s: Tally of zero interpreted as 010000(4096)\n",
                   __func__);
        tally = 4096;
      }

    sim_debug (DBG_DEBUG, & disk_dev,
               "%s: Tally %d (%o)\n", __func__, tally, tally);

    // Seek specific processing

    if (tally != 1)
      {
        sim_printf ("disk seek dazed by tally %d != 1\n", tally);
        p -> stati = 04510; // Cmd reject, invalid inst. seq.
        p -> chanStatus = chanStatIncorrectDCW;
        return -1;
      }

    word36 seekData;
    core_read (daddr, & seekData, "Disk seek address");

//sim_printf ("seekData %012llo\n", seekData);
// Observations about the seek/write stream
// the stream is seek512 followed by a write 1024.
// the seek data is:  000300nnnnnn
// lets assume the 3 is a copy of the seek cmd # as a data integrity check.
// highest observed n during vol. inoit. 272657(8) 95663(10)
//

// disk_control.pl1: 
//   quentry.sector = bit (sector, 21);  /* Save the disk device address. */
// suggests seeks are 21 bits.
//  
    disk_statep -> seekPosition = seekData & MASK21;
//sim_printf ("seek seekPosition %d\n", disk_statep -> seekPosition);
    p -> stati = 00000; // Channel ready
    return 0;
  }

static int diskRead (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
                      devices [chan] [p -> IDCW_DEV_CODE];
    uint devUnitIdx = d -> devUnitIdx;
    UNIT * unitp = & disk_unit [devUnitIdx];
    struct disk_state * disk_statep = & disk_states [devUnitIdx];

    sim_debug (DBG_NOTIFY, & disk_dev, "Read %d\n", devUnitIdx);
    disk_statep -> io_mode = read_mode;
    p -> initiate = true;
    p -> stati = 04000;

// Process DDCW

    bool ptro;
    bool send;
    bool uff;
    int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
    if (rc < 0)
      {
        sim_printf ("diskRead list service failed\n");
        return -1;
      }
    if (uff)
      {
        sim_printf ("diskRead ignoring uff\n"); // XXX
      }
    if (! send)
      {
        sim_printf ("diskRead nothing to send\n");
        return 1;
      }
    if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
      {
        sim_printf ("diskRead expected DDCW\n");
        return -1;
      }


    uint tally = p -> DDCW_TALLY;
    uint daddr = p -> DDCW_ADDR;
    if (disk_statep -> mask)
      daddr |= ((disk_statep -> ext) & MASK6) << 18;
    if (tally == 0)
      {
        sim_debug (DBG_DEBUG, & disk_dev,
                   "%s: Tally of zero interpreted as 010000(4096)\n",
                   __func__);
        tally = 4096;
      }

    sim_debug (DBG_DEBUG, & disk_dev,
               "%s: Tally %d (%o)\n", __func__, tally, tally);

    rc = fseek (unitp -> fileref, 
                disk_statep -> seekPosition * SECTOR_SZ_IN_BYTES,
                SEEK_SET);
    if (rc)
      {
        sim_printf ("fseek (read) returned %d, errno %d\n", rc, errno);
        p -> stati = 04202; // attn, seek incomplete
        return -1;
      }

    // Convert from word36 format to packed72 format

    // round tally up to sector boundary
    
    // this math assumes tally is even.
   
    uint tallySectors = (tally + SECTOR_SZ_IN_W36 - 1) / 
                         SECTOR_SZ_IN_W36;
    uint tallyWords = tallySectors * SECTOR_SZ_IN_W36;
    //uint tallyBytes = tallySectors * SECTOR_SZ_IN_BYTES;
    uint p72ByteCnt = (tallyWords * 36) / 8;
    uint8 buffer [p72ByteCnt];
    memset (buffer, 0, sizeof (buffer));
    sim_debug (DBG_TRACE, & disk_dev, "Disk read  %3d %8d %3d\n",
               devUnitIdx, disk_statep -> seekPosition, tallySectors);
//sim_printf ("Disk read  %8d %3d %08o\n",
        //disk_statep -> seekPosition, tallySectors, daddr);

    rc = fread (buffer, SECTOR_SZ_IN_BYTES,
                tallySectors,
                unitp -> fileref);

    if (rc == 0) // eof; reading a sector beyond the high water mark.
      {
        // okay; buffer was zero, so just pretend that a zero filled
        // sector was read (ala demand page zero)
      }
    else if (rc != (int) tallySectors)
      {
        sim_printf ("read returned %d, errno %d\n", rc, errno);
        p -> stati = 04202; // attn, seek incomplete
        p -> chanStatus = chanStatIncorrectDCW;
        return -1;
      }
//sim_printf ("tallySectors %u\n", tallySectors);
//sim_printf ("p72ByteCnt %u\n", p72ByteCnt);
//for (uint i = 0; i < p72ByteCnt; i += 9)
//{ word36 w1 = extr (& buffer [i / 9], 0, 36);
  //word36 w2 = extr (& buffer [i / 9], 36, 36);
  //sim_printf ("%5d %012llo %012llo\n", i * 2 / 9, w1, w2);
//}
//sim_printf ("read seekPosition %d\n", disk_statep -> seekPosition);
//sim_printf ("buffer 0...\n");
//for (uint i = 0; i < 9; i ++) sim_printf (" %03o", buffer [i]);
//sim_printf ("\n");
    disk_statep -> seekPosition += tallySectors;

    uint wordsProcessed = 0;
    for (uint i = 0; i < tally; i ++)
      {
        word36 w;
        extractWord36FromBuffer (buffer, p72ByteCnt, & wordsProcessed,
                                 & w);
        core_write (daddr + i, w, "Disk read");
        p -> isOdd = (daddr + i) % 2;
      }
//for (uint i = 0; i < tally; i ++) sim_printf ("%8o %012llo\n", daddr + i, M [daddr + i]);
    return 0;
  }

static int diskWrite (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
                      devices [chan] [p -> IDCW_DEV_CODE];
    uint devUnitIdx = d -> devUnitIdx;
    UNIT * unitp = & disk_unit [devUnitIdx];
    struct disk_state * disk_statep = & disk_states [devUnitIdx];

    sim_debug (DBG_NOTIFY, & disk_dev, "Read %d\n", devUnitIdx);
    disk_statep -> io_mode = read_mode;
    p -> initiate = true;
    p -> stati = 04000;

// Process DDCW

    bool ptro;
    bool send;
    bool uff;
    int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
    if (rc < 0)
      {
        sim_printf ("diskWrite list service failed\n");
        return -1;
      }
    if (uff)
      {
        sim_printf ("diskWrite ignoring uff\n"); // XXX
      }
    if (! send)
      {
        sim_printf ("diskWrite nothing to send\n");
        return 1;
      }
    if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
      {
        sim_printf ("diskWrite expected DDCW\n");
        return -1;
      }


    uint tally = p -> DDCW_TALLY;
    uint daddr = p -> DDCW_ADDR;
    if (disk_statep -> mask)
      daddr |= ((disk_statep -> ext) & MASK6) << 18;
    if (tally == 0)
      {
        sim_debug (DBG_DEBUG, & disk_dev,
                   "%s: Tally of zero interpreted as 010000(4096)\n",
                   __func__);
        tally = 4096;
      }

    sim_debug (DBG_DEBUG, & disk_dev,
               "%s: Tally %d (%o)\n", __func__, tally, tally);

    rc = fseek (unitp -> fileref, 
                disk_statep -> seekPosition * SECTOR_SZ_IN_BYTES,
                SEEK_SET);
    if (rc)
      {
        sim_printf ("fseek (read) returned %d, errno %d\n", rc, errno);
        p -> stati = 04202; // attn, seek incomplete
        return -1;
      }

    // Convert from word36 format to packed72 format

    // round tally up to sector boundary
    
    // this math assumes tally is even.
   
    uint tallySectors = (tally + SECTOR_SZ_IN_W36 - 1) / 
                         SECTOR_SZ_IN_W36;
    uint tallyWords = tallySectors * SECTOR_SZ_IN_W36;
    //uint tallyBytes = tallySectors * SECTOR_SZ_IN_BYTES;
    uint p72ByteCnt = (tallyWords * 36) / 8;
    uint8 buffer [p72ByteCnt];
    memset (buffer, 0, sizeof (buffer));
    uint wordsProcessed = 0;
    for (uint i = 0; i < tally; i ++)
      {
        word36 w;
        core_read (daddr + i, & w, "Disk write");
        insertWord36toBuffer (buffer, p72ByteCnt, & wordsProcessed,
                              w);
        p -> isOdd = (daddr + i) % 2;
      }

    sim_debug (DBG_TRACE, & disk_dev, "Disk write %3d %8d %3d\n",
               devUnitIdx, disk_statep -> seekPosition, tallySectors);
    rc = fwrite (buffer, SECTOR_SZ_IN_BYTES,
                 tallySectors,
                 unitp -> fileref);
//sim_printf ("Disk write %8d %3d %08o\n",
//disk_statep -> seekPosition, tallySectors, daddr);
              
    if (rc != (int) tallySectors)
      {
        sim_printf ("fwrite returned %d, errno %d\n", rc, errno);
        p -> stati = 04202; // attn, seek incomplete
        p -> chanStatus = chanStatIncorrectDCW;
        return -1;
      }

    disk_statep -> seekPosition += tallySectors;

    return 0;
  }

static int disk_cmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
                      devices [chan] [p -> IDCW_DEV_CODE];
    uint devUnitIdx = d -> devUnitIdx;
    UNIT * unitp = & disk_unit [devUnitIdx];
    struct disk_state * disk_statep = & disk_states [devUnitIdx];

    disk_statep -> io_mode = no_mode;
    switch (p -> IDCW_DEV_CMD)
      {
        case 025: // CMD 25 READ
          {
            int rc = diskRead (iomUnitIdx, chan);
            if (rc)
              return -1;
          }
          break;

        case 030: // CMD 30 SEEK_512
          {
            int rc = diskSeek512 (iomUnitIdx, chan);
            if (rc)
              return -1;
          }
          break;

        case 031: // CMD 31 WRITE
          {
            int rc = diskWrite (iomUnitIdx, chan);
            if (rc)
              return -1;

            sim_debug (DBG_NOTIFY, & disk_dev, "Write %d\n", devUnitIdx);
            p -> isRead = false;
            disk_statep -> io_mode = write_mode;
            p -> initiate = true;
//sim_printf ("disk write [%lld]\n", sim_timell ());
            p -> stati = 04000;
          }
//exit(1);
          break;

// dcl  1 io_status_word based (io_status_word_ptr) aligned,       /* I/O status information */
//   (
//   2 t bit (1),              /* set to "1"b by IOM */
//   2 power bit (1),          /* non-zero if peripheral absent or power off */
//   2 major bit (4),          /* major status */
//   2 sub bit (6),            /* substatus */
//   2 eo bit (1),             /* even/odd bit */
//   2 marker bit (1),         /* non-zero if marker status */
//   2 soft bit (2),           /* software status */
//   2 initiate bit (1),       /* initiate bit */
//   2 abort bit (1),          /* software abort bit */
//   2 channel_stat bit (3),   /* IOM channel status */
//   2 central_stat bit (3),   /* IOM central status */
//   2 mbz bit (6),
//   2 rcount bit (6)
//   ) unaligned;              /* record count residue */

        case 040: // CMD 40 Reset status
          {
            p -> stati = 04000;
            disk_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & disk_dev, "Reset status %d\n", devUnitIdx);
            p -> initiate = false;
          }
          break;


        default:
          {
fail:
            p -> stati = 04501;
            sim_debug (DBG_ERR, & disk_dev,
                       "%s: Unknown command 0%o\n", __func__, p -> IDCW_DEV_CMD);
            p -> chanStatus = chanStatIncorrectDCW;
sim_printf ("%s: Unknown command 0%o\n", __func__, p -> IDCW_DEV_CMD);
            break;
          }
      }
#ifdef IOM2
    int devUnitIdx = DISK_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToDsk [devUnitIdx] . iomUnitIdx;
    struct disk_state * disk_statep = & disk_states [devUnitIdx];
    * disc = false;

// init_toehold.pl1:
/* write command = "31"b3; read command = "25"b3 */
//  pcw.command = "40"b3;                      /* reset status */

    int chan = pcwp-> chan;

    disk_statep -> chan = pcwp -> chan;
    disk_statep -> mask = pcwp -> mask;
    disk_statep -> ext = pcwp -> ext;

//sim_printf ("disk_cmd %o [%lld]\n", pcwp -> dev_cmd, sim_timell ());
    iomChannelData_ * p = & iomChannelData [iomUnitIdx] [chan];
    if (p -> ptp)
      sim_err ("PTP in disk\n");
    p -> stati = 0;

    switch (pcwp -> dev_cmd)
      {
        case 000: // CMD 00 Request status
          {
            p -> stati = 04000;
            disk_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & disk_dev, "Request status %d\n", devUnitIdx);
            p -> initiate = true;
            * disc = true;
          }
          break;

        case 022: // CMD 22 Read Status Resgister
          {
            sim_debug (DBG_NOTIFY, & disk_dev, "Read Status Register %d\n", devUnitIdx);
            disk_statep -> io_mode = request_status_mode;
            p -> initiate = true;
            p -> stati = 04000;
          }
          break;

        case 034: // CMD 34 SEEK
          {
//sim_printf ("disk seek [%lld]\n", sim_timell ());
            sim_debug (DBG_NOTIFY, & disk_dev, "Seek %d\n", devUnitIdx);
            disk_statep -> io_mode = seek_mode;
            p -> initiate = true;
          }
          break;

        case 042: // CMD 42 RESTORE
          {
            sim_debug (DBG_NOTIFY, & disk_dev, "Restore %d\n", devUnitIdx);
            disk_statep -> io_mode = no_mode;
            p -> stati = 04000;
            * disc = true;
          }
          break;

        default:
          {
sim_printf ("disk daze %o\n", pcwp -> dev_cmd);
            p -> stati = 04501; // cmd reject, invalid opcode
            disk_statep -> io_mode = no_mode;
            p -> chanStatus = chanStatIncorrectDCW;
            * disc = true;
          }
          break;
      
      }
    //status_service (iomUnitIdx, chan, false);
#endif
    return 0;
  }

#ifdef IOM2
static int disk_ddcw (UNIT * unitp, dcw_t * ddcwp)
  {
    int devUnitIdx = DISK_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToDsk [devUnitIdx] . iomUnitIdx;

    struct disk_state * disk_statep = & disk_states [devUnitIdx];
    iomChannelData_ * p = & iomChannelData [iomUnitIdx] [disk_statep -> chan];
    switch (disk_statep -> io_mode)
      {
        case no_mode:
          {
            sim_debug (DBG_ERR, & disk_dev, "DDCW when io_mode == no_mode\n");
            p -> stati = 05001; // BUG: arbitrary error code; config switch
            p -> chanStatus = chanStatIncorrectDCW;
          }
          break;

        case seek512_mode:
          {
            uint tally = ddcwp -> fields.ddcw.tally;
            uint daddr = ddcwp -> fields.ddcw.daddr;
            if (disk_statep -> mask)
              daddr |= ((disk_statep -> ext) & MASK6) << 18;
            // uint cp = ddcwp -> fields.ddcw.cp;

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            // Seek specific processing

            if (tally != 1)
              {
                sim_printf ("disk seek dazed by tally %d != 1\n", tally);
                p -> stati = 04510; // Cmd reject, invalid inst. seq.
                p -> chanStatus = chanStatIncorrectDCW;
                break;
              }

            word36 seekData;
            core_read (daddr, & seekData, "Disk seek address");

//sim_printf ("seekData %012llo\n", seekData);
// Observations about the seek/write stream
// the stream is seek512 followed by a write 1024.
// the seek data is:  000300nnnnnn
// lets assume the 3 is a copy of the seek cmd # as a data integrity check.
// highest observed n during vol. inoit. 272657(8) 95663(10)
//

// disk_control.pl1: 
//   quentry.sector = bit (sector, 21);  /* Save the disk device address. */
// suggests seeks are 21 bits.
//  
            disk_statep -> seekPosition = seekData & MASK21;
//sim_printf ("seek seekPosition %d\n", disk_statep -> seekPosition);
            p -> stati = 00000; // Channel ready
          }
          break;

        case seek_mode:
          {
            sim_printf ("disk seek not here yet\n");
            p -> stati = 04510; // Cmd reject, invalid inst. seq.
            p -> chanStatus = chanStatIncorrectDCW;
          }
          break;

        case read_mode:
          {
//sim_printf ("disk read [%lld]\n", sim_timell ());
            uint tally = ddcwp -> fields.ddcw.tally;
            uint daddr = ddcwp -> fields.ddcw.daddr;
            if (disk_statep -> mask)
              {
                //sim_printf ("mask: daddr was %o; ext %o\n", daddr, disk_statep -> ext);
                daddr |= ((disk_statep -> ext) & MASK6) << 18;
                //sim_printf ("mask: daddr now %o\n", daddr);
               }

            // uint cp = ddcwp -> fields.ddcw.cp;

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

//sim_printf ("tally %d\n", tally);

            int rc = fseek (unitp -> fileref, 
                        disk_statep -> seekPosition * SECTOR_SZ_IN_BYTES,
                        SEEK_SET);
            if (rc)
              {
                sim_printf ("fseek (read) returned %d, errno %d\n", rc, errno);
                p -> stati = 04202; // attn, seek incomplete
                break;
              }

            // Convert from word36 format to packed72 format

            // round tally up to sector boundary
            
            // this math assumes tally is even.
           
            uint tallySectors = (tally + SECTOR_SZ_IN_W36 - 1) / 
                                SECTOR_SZ_IN_W36;
            uint tallyWords = tallySectors * SECTOR_SZ_IN_W36;
            //uint tallyBytes = tallySectors * SECTOR_SZ_IN_BYTES;
            uint p72ByteCnt = (tallyWords * 36) / 8;
            uint8 buffer [p72ByteCnt];
            memset (buffer, 0, sizeof (buffer));
            sim_debug (DBG_TRACE, & disk_dev, "Disk read  %3d %8d %3d\n",
                       devUnitIdx, disk_statep -> seekPosition, tallySectors);
//sim_printf ("Disk read  %8d %3d %08o\n",
        //disk_statep -> seekPosition, tallySectors, daddr);

            rc = fread (buffer, SECTOR_SZ_IN_BYTES,
                        tallySectors,
                        unitp -> fileref);

            if (rc == 0) // eof; reading a sector beyond the high water mark.
              {
                // okay; buffer was zero, so just pretend that a zero filled
                // sector was read (ala demand page zero)
              }
            else if (rc != (int) tallySectors)
              {
                sim_printf ("read returned %d, errno %d\n", rc, errno);
                p -> stati = 04202; // attn, seek incomplete
                p -> chanStatus = chanStatIncorrectDCW;
                break;
              }
//sim_printf ("tallySectors %u\n", tallySectors);
//sim_printf ("p72ByteCnt %u\n", p72ByteCnt);
//for (uint i = 0; i < p72ByteCnt; i += 9)
//{ word36 w1 = extr (& buffer [i / 9], 0, 36);
  //word36 w2 = extr (& buffer [i / 9], 36, 36);
  //sim_printf ("%5d %012llo %012llo\n", i * 2 / 9, w1, w2);
//}
//sim_printf ("read seekPosition %d\n", disk_statep -> seekPosition);
//sim_printf ("buffer 0...\n");
//for (uint i = 0; i < 9; i ++) sim_printf (" %03o", buffer [i]);
//sim_printf ("\n");
            disk_statep -> seekPosition += tallySectors;

            uint wordsProcessed = 0;
            for (uint i = 0; i < tally; i ++)
              {
                word36 w;
                extractWord36FromBuffer (buffer, p72ByteCnt, & wordsProcessed,
                                         & w);
                core_write (daddr + i, w, "Disk read");
                p -> isOdd = (daddr + i) % 2;
              }
//for (uint i = 0; i < tally; i ++) sim_printf ("%8o %012llo\n", daddr + i, M [daddr + i]);
          }
          break;

        case write_mode:
          {
            uint tally = ddcwp -> fields.ddcw.tally;
            uint daddr = ddcwp -> fields.ddcw.daddr;
            if (disk_statep -> mask)
              daddr |= ((disk_statep -> ext) & MASK6) << 18;
            // uint cp = ddcwp -> fields.ddcw.cp;

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

//sim_printf ("tally %d\n", tally);

            int rc = fseek (unitp -> fileref, 
                        disk_statep -> seekPosition * SECTOR_SZ_IN_BYTES,
                        SEEK_SET);
//sim_printf ("write seekPosition %d\n", disk_statep -> seekPosition);
            if (rc)
              {
                sim_printf ("fseek (write) returned %d, errno %d\n", rc, errno);
                p -> stati = 04202; // attn, seek incomplete
                p -> chanStatus = chanStatIncorrectDCW;
                break;
              }

            // Convert from word36 format to packed72 format

            // round tally up to sector boundary
            
            // this math assumes tally is even.
           
            uint tallySectors = (tally + SECTOR_SZ_IN_W36 - 1) / 
                                SECTOR_SZ_IN_W36;
            uint tallyWords = tallySectors * SECTOR_SZ_IN_W36;
            //uint tallyBytes = tallySectors * SECTOR_SZ_IN_BYTES;
            uint p72ByteCnt = (tallyWords * 36) / 8;
            uint8 buffer [p72ByteCnt];
            memset (buffer, 0, sizeof (buffer));
            uint wordsProcessed = 0;
            for (uint i = 0; i < tally; i ++)
              {
                word36 w;
                core_read (daddr + i, & w, "Disk write");
                insertWord36toBuffer (buffer, p72ByteCnt, & wordsProcessed,
                                      w);
                p -> isOdd = (daddr + i) % 2;
              }

            sim_debug (DBG_TRACE, & disk_dev, "Disk write %3d %8d %3d\n",
                       devUnitIdx, disk_statep -> seekPosition, tallySectors);
            rc = fwrite (buffer, SECTOR_SZ_IN_BYTES,
                         tallySectors,
                         unitp -> fileref);
//sim_printf ("Disk write %8d %3d %08o\n",
        //disk_statep -> seekPosition, tallySectors, daddr);
                      
            if (rc != (int) tallySectors)
              {
                sim_printf ("fwrite returned %d, errno %d\n", rc, errno);
                p -> stati = 04202; // attn, seek incomplete
                p -> chanStatus = chanStatIncorrectDCW;
                break;
              }

            disk_statep -> seekPosition += tallySectors;

          }
          break;

        case request_status_mode:
          {
            uint tally = ddcwp -> fields.ddcw.tally;
            uint daddr = ddcwp -> fields.ddcw.daddr;
            if (disk_statep -> mask)
              daddr |= ((disk_statep -> ext) & MASK6) << 18;
            // uint cp = ddcwp -> fields.ddcw.cp;

            if (tally != 4)
              {
                sim_debug (DBG_ERR, &iom_dev, 
                  "%s: RSR expected tally of 4, is %d\n",
                   __func__, tally);
              }
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

// XXX need status register data format 
            sim_debug (DBG_ERR, & disk_dev, "Need status register data format\n");
            for (uint i = 0; i < tally; i ++)
              //M [daddr + i] = 0;
              core_write (daddr + i, 0, "Disk status register");

            //M [daddr] = SIGN36;
            core_write (daddr, SIGN36, "Disk status register");

          }
          break;
      }
    return 0;
  }
#endif

int disk_iom_cmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
// Is it an IDCW?

    if (p -> DCW_18_20_CP == 7)
      {

        // Ignore a CMD 051 in the PCW
        if (p -> IDCW_DEV_CMD == 051)
          return 1;

        disk_cmd (iomUnitIdx, chan);
      }
    else // DDCW/TDCW
      {
        sim_printf ("disk_iom_cmd expected IDCW\n");
        return -1;
      }
    return 0;
#ifdef IOM2
    int devUnitIdx = DISK_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToDsk [devUnitIdx] . iomUnitIdx;

    // First, execute the command in the PCW, and then walk the 
    // payload channel mbx looking for IDCWs.

    // uint chanloc = mbx_loc (iomUnitIdx, pcwp -> chan);
    //lpw_t lpw;
    //fetch_and_parse_lpw (& lpw, chanloc, false);

// Ignore the entire operation is a CMD 051 is in the PCW
    if (pcwp -> dev_cmd == 051)
      return 1;

    // disc is set by disk_cmd if the IDCW does not expect DDCWS; which
    // implies means that if disc & ctrl !=2, then the list is done.
    bool disc;
    disk_cmd (unitp, pcwp, & disc);

    // ctrl of the pcw is observed to be 0 even when there are idcws in the
    // list so ignore that and force it to 2.
    //uint ctrl = pcwp -> control;
    uint ctrl = 2;
    int ptro = 0;
    for (;;)
      {
//sim_printf ("perusing channel mbx lpw....\n");
        dcw_t dcw;
        int rc = iomListService (iomUnitIdx, pcwp -> chan, & dcw, & ptro);
        if (rc)
          {
//sim_printf ("list service denies!\n");
            break;
          }
//sim_printf ("persuing got type %d\n", dcw . type);
#if 0
        if (dcw . type != idcw)
          {
// 04501 : COMMAND REJECTED, invalid command
            iomChannelData_ * p = & iomChannelData [iomUnitIdx] [pcwp -> chan];
            p -> stati = 04501; 
            p -> dev_code = dcw . fields . instr. dev_code;
            p -> chanStatus = chanStatInvalidInstrPCW;
            //status_service (iomUnitIdx, pcwp -> chan, false);
            break;
          }
#endif


        if (dcw . type == idcw)
          {
            // The dcw does not necessarily have the same dev_code as the pcw....

            devUnitIdx = findDiskUnit (iomUnitIdx, pcwp -> chan, dcw . fields . instr. dev_code);
            if (devUnitIdx < 0)
              {
// 04502 : COMMAND REJECTED, invalid device code
                iomChannelData_ * p = & iomChannelData [iomUnitIdx] [pcwp -> chan];
                p -> stati = 04502; 
                p -> dev_code = dcw . fields . instr. dev_code;
                p -> chanStatus = chanStatInvalidInstrPCW;
                //status_service (iomUnitIdx, pcwp -> chan, false);
                break;
              }
            unitp = & disk_unit [devUnitIdx];
            disk_cmd (unitp, & dcw . fields . instr, & disc);
            ctrl = dcw . fields . instr . control;
            if (ctrl != 2 && disc)
              break;
          }
        else if (dcw . type == ddcw)
          {
            unitp = & disk_unit [devUnitIdx];
            disk_ddcw (unitp, & dcw);
            if (ctrl == 0 && dcw . fields . ddcw . type == 0) // IOTD
              break;
          }
      }
//sim_printf ("disk interrupts\n");
    send_terminate_interrupt (iomUnitIdx, pcwp -> chan);

    return 1;
#endif
  }

static t_stat disk_svc (UNIT * unitp)
  {
#ifdef IOM2
#if 1
    int diskUnitNum = DISK_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToDsk [diskUnitNum] . iomUnitIdx;
    int chanNum = cables -> cablesFromIomToDsk [diskUnitNum] . chan_num;
    pcw_t * pcwp = & iomChannelData [iomUnitIdx] [chanNum] . pcw;
    disk_iom_cmd (unitp, pcwp);
#else
    int devUnitIdx = DISK_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToDsk [devUnitIdx] . iomUnitIdx;
    word24 dcw_ptr = (word24) (unitp -> u3);
    pcw_t pcw;
    word36 word0, word1;
    
    (void) fetch_abs_pair (dcw_ptr, & word0, & word1);
    decode_idcw (iomUnitIdx, & pcw, 1, word0, word1);
    disk_iom_cmd (unitp, & pcw);
#endif 
#endif 
    return SCPE_OK;
  }


static t_stat disk_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED void * desc)
  {
    sim_printf("Number of DISK units in system is %d\n", disk_dev . numunits);
    return SCPE_OK;
  }

static t_stat disk_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_DISK_UNITS_MAX)
      return SCPE_ARG;
    disk_dev . numunits = n;
    return SCPE_OK;
  }

void loadDisk (uint driveNumber, char * diskFilename)
  {
    //sim_printf ("in loadTape %d %s\n", driveNumber, tapeFilename);
    t_stat stat = attach_unit (& disk_unit [driveNumber], diskFilename);
    if (stat != SCPE_OK)
      {
        sim_printf ("loadDisk sim_disk_attach returned %d\n", stat);
        return;
      }

// if substr (special_status_word, 20, 1) ^= "1"b | substr (special_status_word, 13, 6) ^= "00"b3
// if substr (special_status_word, 34, 3) ^= "001"b
// Note the 34,3 spans 34,35,36; therefore the bits are 1..36, not 0..35
// 20,1 is bit 19
// 13,6, is bits 12..17
// status0 is 19..26
// status1 is 28..35
// so substr (w, 20, 1) is bit 0 of status0
//    substr (w, 13, 6) is the low 6 bits of dev_no
//    substr (w, 34, 3) is the low 3 bits of status 1
    //sim_printf ("%s %d %o\n", tapeFilename, ro,  mt_unit [driveNumber] . flags);
    //sim_printf ("special int %d %o\n", driveNumber, mt_unit [driveNumber] . flags);
    send_special_interrupt (cables -> cablesFromIomToDsk [driveNumber] . iomUnitIdx,
                            cables -> cablesFromIomToDsk [driveNumber] . chan_num,
                            cables -> cablesFromIomToDsk [driveNumber] . dev_code,
                            0x40, 01 /* disk pack ready */);
  }

t_stat attachDisk (char * label)
  {
    //sim_printf ("%s %s %s\n", label, withring ? "rw" : "ro", drive);
    int i;
    for (i = 1; i < N_DISK_UNITS_MAX; i ++)
      {
sim_printf ("%d fileref %p filename %s\n", i, disk_unit [i] . fileref, disk_unit [i] . filename);
        if (disk_unit [i] . fileref == NULL)
          break;
      }
    if (i >= N_DISK_UNITS_MAX)
      {
        sim_printf ("can't find available disk drive\n");
        return SCPE_ARG;
      }
    loadDisk (i, label);
    return SCPE_OK;
  }

