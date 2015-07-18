// Devices connected to an IOM (I/O multiplexer) (possibly indirectly)
enum dev_type { DEVT_NONE = 0, DEVT_TAPE, DEVT_CON, DEVT_DISK, DEVT_MPC, DEVT_DN355, DEVT_CRDRDR };
typedef enum chan_type { chan_type_CPI, chan_type_PSI } chan_type;

struct iom
  {
    struct devices
      {
        enum dev_type type;
        enum chan_type ctype;
        DEVICE * dev; // attached device; points into sim_devices[]
        uint devUnitNum; // Which unit of the attached device
        UNIT * board;  // points into iomUnit
        iomCmd * iomCmd;
      } devices [MAX_CHANNELS] [N_DEV_CODES];
  };

extern struct iom iom [N_IOM_UNITS_MAX];

struct cpu_array
  {
    struct
      {
        bool inuse;
        int scu_unit_num; // 
        DEVICE * devp;
      } ports [N_CPU_PORTS];

  };

extern struct cpu_array cpu_array [N_CPU_UNITS_MAX];

struct cable_from_iom_to_crdrdr
  {
    int iom_unit_num;
    int chan_num;
    int dev_code;
  };

extern struct cable_from_iom_to_crdrdr cables_from_ioms_to_crdrdr [N_CRDRDR_UNITS_MAX];

struct cable_from_iom_to_fnp
  {
    int iomUnitNum;
    int chan_num;
    int dev_code;
  };
extern struct cable_from_iom_to_fnp  cables_from_ioms_to_fnp [N_FNP_UNITS_MAX];

struct cable_from_iom_to_disk
  {
    int iom_unit_num;
    int chan_num;
    int dev_code;
  };
extern struct cable_from_iom_to_disk cables_from_ioms_to_disk [N_DISK_UNITS_MAX];

struct cable_from_iom_to_con
  {
    int iom_unit_num;
    int chan_num;
    int dev_code;
  };

extern struct cable_from_iom_to_con cables_from_ioms_to_con [N_OPCON_UNITS_MAX];

struct cable_from_ioms_to_mt
  {
    int iom_unit_num;
    int chan_num;
    int dev_code;
  };

extern struct cable_from_ioms_to_mt cables_from_ioms_to_mt [N_MT_UNITS_MAX];

struct cablesFromScu
  {
    bool inuse;
    int scuUnitNum;
    int scuPortNum;
  };

extern struct cablesFromScu cablesFromScus [N_IOM_UNITS_MAX] [N_IOM_PORTS];

struct cable_from_cpu
  {
    int cpu_unit_num;
    int cpu_port_num;
  };

extern struct cable_from_cpu cables_from_cpus [N_SCU_UNITS_MAX] [N_SCU_PORTS];


t_stat sys_cable (UNUSED int32 arg, char * buf);
void sysCableInit (void);
