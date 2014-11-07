// Multipass data

#ifdef MULTIPASS

typedef struct multipassStats
  {
    struct _ppr PPR;
    word36 inst;

    word36 A, Q, E, X [8], IR, TR, RALR;
    struct _par PAR [8];
    //struct _bar BAR;
    word3 TRR;
    word15 TSR;
    word6 TBR;
    word18 CA;

    struct _dsbr DSBR;

    _fault faultNumber;
    _fault_subtype subFault;

    uint intr_pair_addr;
    cycles_t cycle;

    uint64 cycles;


    uint64 diskSeeks;
    uint64 diskWrites;
    uint64 diskReads;

    
  } multipassStats;

extern multipassStats * multipassStatsPtr;

#endif
