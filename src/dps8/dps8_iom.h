// I/O Multiplexer
enum { max_channels = 32 }; // enums are more constant than consts...

// Used to communicate between the IOM and devices
typedef struct {
    int iom_unit_num;
    int chan;
    void* statep; // For use by device specific code
    int dev_cmd; // 6 bits
    // XXX Caution; this is the unit number from the IOM, it is NOT the
    // same as unit number. E.g.
    //    dev_code       simh
    //      0            cardreader[0]
    //      1            tape[0]
    //      2            tape[1]
    //      3            disk[0]

    int dev_code; // 6 bits
    int chan_data; // 6 bits; often some sort of count
    flag_t have_status; // set to true by the device when operation is complete
    int major;
    int substatus;
    flag_t is_read;
    int time; // request by device for queuing via sim_activate()
} chan_devinfo;


