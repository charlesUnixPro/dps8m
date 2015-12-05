struct udpdev
  {
  };

// Prototypes for the RTC module ...
//   I really hate sharing things like this, but it's the only way to get the
// modem transmitter timing exactly right!
//extern uint32 rtc_interval;
//extern t_stat mi_tx_service (uint32 quantum);

// Prototypes for UDP modem/host interface emulation routines ...
#define NOLINK  (-1)
int udp_create (struct udpdev * pdtr, char * premote, int32_t * plink);
int udp_release (struct udpdev * dptr, int32_t link);
int udp_send (struct udpdev * pdtr, int32_t link, uint16_t * pdata, uint16_t count);
int udp_set_link_loopback (struct udpdev * dptr, int32_t link, bool enable_loopback);
int udp_receive (struct udpdev * dptr, int32_t link, uint16_t * pdata, uint16_t maxbufg);

