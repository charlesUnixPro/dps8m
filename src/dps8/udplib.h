/*
 Copyright 2015-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#define NOLINK  (-1)
#define PFLG_FINAL 00001
int udp_create (const char * premote, int32_t * plink);
int udp_release (int32_t link);
int imp_udp_send (int32_t link, uint16_t * pdata, uint16_t count, uint16_t flags);
int imp_udp_receive (int32_t link, uint16_t * pdata, uint16_t maxbufg);
int dn_udp_send (int32_t link, uint8_t * pdata, uint16_t count, uint16_t flags);
int dn_udp_receive (int32_t link, uint8_t * pdata, uint16_t maxbufg);

enum { dn_cmd_bootload = 1, dn_cmd_ISB_IOLD = 2 };

// CS->DN Bootload

struct _dn_bootload
  {
    uint8_t cmd; // 1 dn_cmd_bootload
    //uint64_t /* word36*/  dia_pcw;
  };
typedef struct _dn_bootload dn_bootload;

// CS->DN IDB_IOLD Input Stored Boot IO Load

struct _dn_isd_iold
  {
    uint8_t cmd; // 1 dn_cmd_ISB_IOLD
    //uint64_t /* word36*/  dia_pcw;
  };
typedef struct _dn_isd_iold dn_ids_iold;


