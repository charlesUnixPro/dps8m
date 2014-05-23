void setupOperandDescriptor(int k, EISstruct *e);
void parseNumericOperandDescriptor(int k, EISstruct *e);
void EISwrite49(EISaddr *p, int *pos, int tn, int c49);
void EISloadInputBufferNumeric(DCDstruct *i, int k);

void btd(DCDstruct *i);
void dtb(DCDstruct *i);
void mvne(DCDstruct *i);
void mve(DCDstruct *i);
void mlr(DCDstruct *i);
void mrl(DCDstruct *i);
void mvt(DCDstruct *i);
void scm(DCDstruct *i);
void scmr(DCDstruct *i);
void tct(DCDstruct *i);
void tctr(DCDstruct *i);
void cmpc(DCDstruct *i);
void scd(DCDstruct *i);
void scdr(DCDstruct *i);
void cmpb(DCDstruct *i);
void csl(DCDstruct *i);
void csr(DCDstruct *i);
void sztl(DCDstruct *i);
void sztr(DCDstruct *i);


