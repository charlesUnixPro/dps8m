// Devices connected to an IOM (I/O multiplexer) (possibly indirectly)
// DEVT_NONE must be zero for memset to init it properly.
typedef enum devType
  {
     DEVT_NONE = 0, DEVT_TAPE, DEVT_CON, DEVT_DISK, 
     DEVT_MPC, DEVT_DN355, DEVT_CRDRDR, DEVT_CRDPUN, DEVT_PRT, DEVT_URP
  } devType;

typedef enum chanType { chanTypeCPI, chanTypePSI, chanTypeDirect } chanType;

struct cableFromIomToDev
  {
    struct device
      {
        enum devType type;
        enum chanType ctype;
        DEVICE * dev; // attached device; points into sim_devices[]
        uint devUnitIdx; // simh unit of the attached device
        UNIT * board;  // points into iomUnit
        iomCmd * iomCmd;
      } devices [MAX_CHANNELS] [N_DEV_CODES];
  };

struct cableFromScuToCpu
  {
    struct cpuPort
      {
        bool inuse;
        int scu_unit_num; // 
        DEVICE * devp;
      } ports [N_CPU_PORTS];

  };

struct cableFromIom
  {
    int iomUnitIdx;
    int chan_num;
    int dev_code;
  };

struct cablesFromScu
  {
    bool inuse;
    int scuUnitNum;
    int scuPortNum;
  };

struct cableFromCpu
  {
    int cpu_unit_num;
    int cpu_port_num;
  };

struct cables_t
  {
    // CPU -> SCU
    struct cableFromCpu cablesFomCpu [N_SCU_UNITS_MAX] [N_SCU_PORTS];

    // IOM -> SCU
    struct cablesFromScu cablesFromScus [N_IOM_UNITS_MAX] [N_IOM_PORTS];

    // IOM -> device
    struct cableFromIom cablesFromIomToUrp [N_URP_UNITS_MAX];
    struct cableFromIom cablesFromIomToCrdRdr [N_CRDRDR_UNITS_MAX];
    struct cableFromIom cablesFromIomToCrdPun [N_CRDPUN_UNITS_MAX];
    struct cableFromIom cablesFromIomToPrt [N_PRT_UNITS_MAX];
    struct cableFromIom cablesFromIomToFnp [N_FNP_UNITS_MAX];
    struct cableFromIom cablesFromIomToDsk [N_DISK_UNITS_MAX];
    struct cableFromIom cablesFromIomToCon [N_OPCON_UNITS_MAX];
    struct cableFromIom cablesFromIomToTap [N_MT_UNITS_MAX];
    struct cableFromIom cablesFromIomToAbsi [N_ABSI_UNITS_MAX];

    // SCU -> CPU
    struct cableFromScuToCpu cablesFromScuToCpu [N_CPU_UNITS_MAX];

    // SCU -> IOM
    struct cableFromIomToDev cablesFromIomToDev [N_IOM_UNITS_MAX];
  };

extern struct cables_t * cables;

t_stat sys_cable (UNUSED int32 arg, char * buf);
void sysCableInit (void);
