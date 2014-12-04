// IPC for FNP process
#include "dps8.h"
#include "fnpp.h"
#include "dps8_utils.h"

// fnp_init() will be called once and only once per emulator execution.

void fnppInit (void)
  {
    //sim_printf ("fnppInit called\n");
  }



// A Multics system may have multiple FNPs connected to. As part of the
// hardware description script (base_system.ini), this routine will be called,
// defining the number of FNPs connected to the system. 

// In general, it should only be called once, but that is not enforced.
// Initially, expect the number of units to be 1. The FNP could be implemented
// as one process/FNP or one process for all FNPs; whichever works best. The
// maximum number of supported units is a #define in dps8_fnp.c.

t_stat fnppSetNunits (UNUSED UNIT * uptr, UNUSED int32 value,
                             char * cptr, UNUSED void * desc)
  {

    int n = atoi (cptr);
    //sim_printf ("fnppSetNunits called; units set to %d\n", n);
    return SCPE_OK;
  }



// fnp_reset will be called as part of the Multics boot sequence; it is the
// equivalent of the reset button being pressed on the FNP as part of the
// system boot procedure.  It may be called multiple times during emulator
// execution.

t_stat fnppReset (UNUSED DEVICE * dptr)
  {
    //sim_printf ("fnppReset called\n");
    return SCPE_OK;
  }


#if 0
// As the IOM processes the channel DCW list, it will generate a call to
// fnppCmd() for each IDCW. Return value 0 means ok; non-zero TBD.

int fnppIDCW (UNUSED UNIT * unitp, uint unitNumber)
   {
     //sim_printf ("fnppIDCW called for unit %d\n", unitNumber);
     return 0;
   }

// As the IOM processes the channel DCW list, it will generate a call to
// fnppCmd() for each IOTx.  Return value 0 means ok; non-zero TBD. 

int fnppIOTx (UNUSED UNIT * unitp, uint unitNumber)
   {
     //sim_printf ("fnppIOTx called for unit %d\n", unitNumber);
     return 0;
   }
#endif

// When the IOM receives a CIOC for the FNP, it will pass the channel number
// to the FNP.

int fnppCIOC (UNUSED UNIT * unitp, uint chanNum)
   {
     //sim_printf ("fnppIDCW called for unit %d\n", unitNumber);
     return 0;
   }

// An interrupt delivery interface is also needed; the FNP has the ability to
// send an interrupt to the IOM. A handler that catches the signal and does the
// appropriate signalling to the emulator needs to be defined. Since the IOM
// isn't an independent process, this probably means setting a flag that
// simh_hooks() catches.

// During Multics' initialization of the FNP, it downloads the firmware into
// the FNP. The firmware is customized to the specific installation, and
// contains configuration data; what kind of adapters are connected to the FNP,
// and what protocol they are using. Initially, this data can be ignored, but
// it might be useful at some point for the FNP emulator to parse the firmware
// to extract the configuration, and adapt its configuration to match. For now,
// the configuration should be a bunch of serial terminals.

