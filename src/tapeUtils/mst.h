// MST -- Standard record format
//
// MULTICS SYSTEM-PROGRAMMERS' MANUAL Section BB.3.01
// PUBLISHED: 06/02/67
//
// Each physical consists of a 256 word (9216-bit) data space
// enclosed by an eight-word header and an eight-word trailer. The total
// record length is then 272 words (9792 bits).
//
// hdr      8 word36 =   288 bits =   36 bytes
// data   256 word36 =  9216 bits = 1152 bytes
// trlr     8 word36 =   288 bits =   36 bytes
//
// hdr      8 word36 =   288 bits =   36 bytes
// data  1024 word36 = 36864 bits = 4608 bytes
// trlr     8 word36 =   288 bits =   36 bytes
//       ----          -----        ----
//
// total 1040 word36 = 38440 bits = 4680 bytes
//

#define mst_blksz_bytes 4680
#define mst_blksz_ascii 4160 // 4680 * 9 / 8
#define mst_blksz_word36 1040
#define mst_datasz_bytes 4608
#define mst_header_sz_words36 8
#define mst_trailer_sz_words36 8
#define mst_datasz_word36 1024
// Header, trailer

#define header_c1  0670314355245
#define header_c2  0512556146073
#define trailer_c1 0107463422532
#define trailer_c2 0265221631704

#define header_sz_words 8
#define data_sz_words 1024


