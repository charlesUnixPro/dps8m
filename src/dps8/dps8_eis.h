void doEIS_CAF (void);
void setupOperandDescriptor(int k, EISstruct *e);
#ifdef EIS_CACHE
void setupOperandDescriptorCache(int k, EISstruct *e);
void cleanupOperandDescriptor(int k, EISstruct *e);
#endif
void parseNumericOperandDescriptor(int k, EISstruct *e);
void EISwrite49(EISaddr *p, int *pos, int tn, int c49);
void EISloadInputBufferNumeric(DCDstruct *i, int k);

void dtb(DCDstruct *i);
//void sztl(DCDstruct *i);
//void sztr(DCDstruct *i);


