/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#define BUFSZ (4096 * 9 / 2)

struct tape_state
  {
    enum { no_mode, read_mode, write_mode, survey_mode } io_mode;
    bool is9;
    uint8 buf [BUFSZ];
    t_mtrlnt tbc; // Number of bytes read into buffer
    uint words_processed; // Number of Word36 processed from the buffer
// XXX bug: 'sim> set tapeN rewind' doesn't reset rec_num
    int rec_num; // track tape position
    char device_name [MAX_DEV_NAME_LEN];
    word16 cntlrAddress;
    word16 cntlrTally;
    int tape_length;
  };

extern struct tape_state tape_states [N_MT_UNITS_MAX];


extern UNIT mt_unit [N_MT_UNITS_MAX];
extern DEVICE tape_dev;
extern UNIT mtp_unit [N_MTP_UNITS_MAX];
extern DEVICE mtp_dev;

void mt_init(void);
int get_mt_numunits (void);
//UNIT * getTapeUnit (uint driveNumber);
//void tape_send_special_interrupt (uint driveNumber);
void loadTape (uint driveNumber, char * tapeFilename, bool ro);
t_stat attachTape (char * label, bool withring, char * drive);
t_stat detachTape (char * drive);
int mt_iom_cmd (uint iomUnitIdx, uint chan);
#ifdef NEW_CABLE
//int mtp_iom_cmd (uint iomUnitIdx, uint chan);
#endif
t_stat mount_tape (int32 arg, const char * buf);

