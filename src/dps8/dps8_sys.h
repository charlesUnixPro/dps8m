// System-wide info and options not tied to a specific CPU, IOM, or SCU
typedef struct {
    int clock_speed;
    // Instructions rccl and rscr allow access to a hardware clock.
    // If zero, the hardware clock returns the real time of day.
    // If non-zero, the clock starts at an arbitrary date and ticks at
    // a rate approximately equal to the given number of instructions
    // per second.
    // Delay times are in cycles; negative for immediate
    struct {
        int connect;    // Delay between CIOC instr & connect channel operation
        int chan_activate;  // Time for a list service to send a DCW
        int boot_time; // delay between CPU start and IOM starting boot process
    } iom_times;
    struct {
        int read;
        int xfer;
    } mt_times;
    bool warn_uninit; // Warn when reading uninitialized memory
} sysinfo_t;

// Statistics
typedef struct {
    struct {
        uint nexec;
        uint nmsec; // FIXME: WARNING: if 32 bits, only good for ~47 days :-)
    } instr[1024];
    t_uint64 total_cycles;      // Used for statistics and for simulated clock
    t_uint64 total_faults [N_FAULTS];
    t_uint64 total_instr;
    t_uint64 total_msec;
    uint n_instr;       // Reset to zero on each call to sim_instr()
} stats_t;


extern word36 *M;
extern stats_t sys_stats;
extern sysinfo_t sys_opts;
extern uint64 sim_deb_start;
extern uint64 sim_deb_stop;
extern uint64 sim_deb_break;
#define NO_SUCH_SEGNO ((uint64) -1ll)
extern uint64 sim_deb_segno;
extern DEVICE *sim_devices[];

char * lookupAddress (word18 segno, word18 offset, char * * compname, word18 * compoffset);
void listSource (char * compname, word18 offset, bool print);
//t_stat computeAbsAddrN (word24 * absAddr, int segno, uint offset);


