#define NOLINK  (-1)
#define PFLG_FINAL 00001
int udp_create (char * premote, int32_t * plink);
int udp_release (int32_t link);
int udp_send (int32_t link, uint16_t * pdata, uint16_t count, uint16_t flags);
int udp_receive (int32_t link, uint16_t * pdata, uint16_t maxbufg);

