// Multipass data

#ifdef MULTIPASS

typedef struct multipassStats
  {
    word36 PPR_PSR_IC;

    t_uint64 cycles;

    t_uint64 diskSeeks;
    t_uint64 diskWrites;
    t_uint64 diskReads;

    
  } multipassStats;

extern multipassStats * multipassStatsPtr;

#endif
