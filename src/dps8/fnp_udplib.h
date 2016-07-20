#define FNP_NOLINK  (-1)
#define FNP_MAXDATA      32768      // longest possible packet (in bytes)
int fnp_udp_create (char * premote, int32_t * plink);
int fnp_udp_release (int32_t link);
int fnp_udp_send (int32_t link, char * pdata, uint16_t count, uint16_t flags);
int fnp_udp_receive (int32_t link, char * pdata, uint16_t maxbufg, uint16_t * flags);

