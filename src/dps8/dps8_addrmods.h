struct modificationContinuation
{
#ifdef NO_REWRITE
    int segment;    // segment of whatever we want to write
    int address;    // address of whatever we'll need to write
    int tally;      // value of tally from dCAF()
    int delta;      // value of delta from sCAF()
    int tb;         // character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes.
    word36 indword; // indirect word
    int tmp18;      // temporary address used by some instructions
#endif
    bool bActive;   // if true then continuation is active and needs to be considered
    int mod;        // which address modification are we continuing
    int cf;         // 3-bit character/byte position value,
};
typedef struct modificationContinuation modificationContinuation;

extern bool directOperandFlag;
extern bool characterOperandFlag;
extern int characterOperandSize;
extern int characterOperandOffset;
extern word36 directOperand;

t_stat doComputedAddressFormation (void);
t_stat cac(void);
extern modificationContinuation _modCont, *modCont;
void doComputedAddressContinuation (void);


