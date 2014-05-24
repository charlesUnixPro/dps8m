// simh only explicitly supports a single cpu
#define N_CPU_UNITS 1
#define CPU_UNIT_NUM 0


#define JMP_ENTRY       0
#define JMP_RETRY       1   ///< retry instruction
#define JMP_NEXT        2   ///< goto next sequential instruction
#define JMP_TRA         3   ///< treat return as if it were a TRA instruction with PPR.IC already set to where to jump to
#define JMP_STOP        4   ///< treat return as if it were an attempt to unravel the stack and gracefully exit out of sim_instr
#define JMP_INTR        5   // Interrupt detected during processing


// MM's opcode stuff ...

// Opcodes with low bit (bit 27) == 0.  Enum value is value of upper 9 bits.
typedef enum {
	opcode0_mme    = 0001U, // (1 decimal)
	opcode0_drl    = 0002U, // (2 decimal)
	opcode0_mme2   = 0004U, // (4 decimal)
	opcode0_mme3   = 0005U, // (5 decimal)
	opcode0_mme4   = 0007U, // (7 decimal)
	opcode0_nop    = 0011U, // (9 decimal)
	opcode0_puls1  = 0012U, // (10 decimal)
	opcode0_puls2  = 0013U, // (11 decimal)
	opcode0_cioc   = 0015U, // (13 decimal)
	opcode0_adlx0  = 0020U, // (16 decimal)
	opcode0_adlx1  = 0021U, // (17 decimal)
	opcode0_adlx2  = 0022U, // (18 decimal)
	opcode0_adlx3  = 0023U, // (19 decimal)
	opcode0_adlx4  = 0024U, // (20 decimal)
	opcode0_adlx5  = 0025U, // (21 decimal)
	opcode0_adlx6  = 0026U, // (22 decimal)
	opcode0_adlx7  = 0027U, // (23 decimal)
	opcode0_ldqc   = 0032U, // (26 decimal)
	opcode0_adl    = 0033U, // (27 decimal)
	opcode0_ldac   = 0034U, // (28 decimal)
	opcode0_adla   = 0035U, // (29 decimal)
	opcode0_adlq   = 0036U, // (30 decimal)
	opcode0_adlaq  = 0037U, // (31 decimal)
	opcode0_asx0   = 0040U, // (32 decimal)
	opcode0_asx1   = 0041U, // (33 decimal)
	opcode0_asx2   = 0042U, // (34 decimal)
	opcode0_asx3   = 0043U, // (35 decimal)
	opcode0_asx4   = 0044U, // (36 decimal)
	opcode0_asx5   = 0045U, // (37 decimal)
	opcode0_asx6   = 0046U, // (38 decimal)
	opcode0_asx7   = 0047U, // (39 decimal)
	opcode0_adwp0  = 0050U, // (40 decimal)
	opcode0_adwp1  = 0051U, // (41 decimal)
	opcode0_adwp2  = 0052U, // (42 decimal)
	opcode0_adwp3  = 0053U, // (43 decimal)
	opcode0_aos    = 0054U, // (44 decimal)
	opcode0_asa    = 0055U, // (45 decimal)
	opcode0_asq    = 0056U, // (46 decimal)
	opcode0_sscr   = 0057U, // (47 decimal)
	opcode0_adx0   = 0060U, // (48 decimal)
	opcode0_adx1   = 0061U, // (49 decimal)
	opcode0_adx2   = 0062U, // (50 decimal)
	opcode0_adx3   = 0063U, // (51 decimal)
	opcode0_adx4   = 0064U, // (52 decimal)
	opcode0_adx5   = 0065U, // (53 decimal)
	opcode0_adx6   = 0066U, // (54 decimal)
	opcode0_adx7   = 0067U, // (55 decimal)
	opcode0_awca   = 0071U, // (57 decimal)
	opcode0_awcq   = 0072U, // (58 decimal)
	opcode0_lreg   = 0073U, // (59 decimal)
	opcode0_ada    = 0075U, // (61 decimal)
	opcode0_adq    = 0076U, // (62 decimal)
	opcode0_adaq   = 0077U, // (63 decimal)
	opcode0_cmpx0  = 0100U, // (64 decimal)
	opcode0_cmpx1  = 0101U, // (65 decimal)
	opcode0_cmpx2  = 0102U, // (66 decimal)
	opcode0_cmpx3  = 0103U, // (67 decimal)
	opcode0_cmpx4  = 0104U, // (68 decimal)
	opcode0_cmpx5  = 0105U, // (69 decimal)
	opcode0_cmpx6  = 0106U, // (70 decimal)
	opcode0_cmpx7  = 0107U, // (71 decimal)
	opcode0_cwl    = 0111U, // (73 decimal)
	opcode0_cmpa   = 0115U, // (77 decimal)
	opcode0_cmpq   = 0116U, // (78 decimal)
	opcode0_cmpaq  = 0117U, // (79 decimal)
	opcode0_sblx0  = 0120U, // (80 decimal)
	opcode0_sblx1  = 0121U, // (81 decimal)
	opcode0_sblx2  = 0122U, // (82 decimal)
	opcode0_sblx3  = 0123U, // (83 decimal)
	opcode0_sblx4  = 0124U, // (84 decimal)
	opcode0_sblx5  = 0125U, // (85 decimal)
	opcode0_sblx6  = 0126U, // (86 decimal)
	opcode0_sblx7  = 0127U, // (87 decimal)
	opcode0_sbla   = 0135U, // (93 decimal)
	opcode0_sblq   = 0136U, // (94 decimal)
	opcode0_sblaq  = 0137U, // (95 decimal)
	opcode0_ssx0   = 0140U, // (96 decimal)
	opcode0_ssx1   = 0141U, // (97 decimal)
	opcode0_ssx2   = 0142U, // (98 decimal)
	opcode0_ssx3   = 0143U, // (99 decimal)
	opcode0_ssx4   = 0144U, // (100 decimal)
	opcode0_ssx5   = 0145U, // (101 decimal)
	opcode0_ssx6   = 0146U, // (102 decimal)
	opcode0_ssx7   = 0147U, // (103 decimal)
	opcode0_adwp4  = 0150U, // (104 decimal)
	opcode0_adwp5  = 0151U, // (105 decimal)
	opcode0_adwp6  = 0152U, // (106 decimal)
	opcode0_adwp7  = 0153U, // (107 decimal)
	opcode0_sdbr   = 0154U, // (108 decimal)
	opcode0_ssa    = 0155U, // (109 decimal)
	opcode0_ssq    = 0156U, // (110 decimal)
	opcode0_sbx0   = 0160U, // (112 decimal)
	opcode0_sbx1   = 0161U, // (113 decimal)
	opcode0_sbx2   = 0162U, // (114 decimal)
	opcode0_sbx3   = 0163U, // (115 decimal)
	opcode0_sbx4   = 0164U, // (116 decimal)
	opcode0_sbx5   = 0165U, // (117 decimal)
	opcode0_sbx6   = 0166U, // (118 decimal)
	opcode0_sbx7   = 0167U, // (119 decimal)
	opcode0_swca   = 0171U, // (121 decimal)
	opcode0_swcq   = 0172U, // (122 decimal)
	opcode0_lpri   = 0173U, // (123 decimal)
	opcode0_sba    = 0175U, // (125 decimal)
	opcode0_sbq    = 0176U, // (126 decimal)
	opcode0_sbaq   = 0177U, // (127 decimal)
	opcode0_cnax0  = 0200U, // (128 decimal)
	opcode0_cnax1  = 0201U, // (129 decimal)
	opcode0_cnax2  = 0202U, // (130 decimal)
	opcode0_cnax3  = 0203U, // (131 decimal)
	opcode0_cnax4  = 0204U, // (132 decimal)
	opcode0_cnax5  = 0205U, // (133 decimal)
	opcode0_cnax6  = 0206U, // (134 decimal)
	opcode0_cnax7  = 0207U, // (135 decimal)
	opcode0_cmk    = 0211U, // (137 decimal)
	opcode0_absa   = 0212U, // (138 decimal)
	opcode0_epaq   = 0213U, // (139 decimal)
	opcode0_sznc   = 0214U, // (140 decimal)
	opcode0_cnaa   = 0215U, // (141 decimal)
	opcode0_cnaq   = 0216U, // (142 decimal)
	opcode0_cnaaq  = 0217U, // (143 decimal)
	opcode0_ldx0   = 0220U, // (144 decimal)
	opcode0_ldx1   = 0221U, // (145 decimal)
	opcode0_ldx2   = 0222U, // (146 decimal)
	opcode0_ldx3   = 0223U, // (147 decimal)
	opcode0_ldx4   = 0224U, // (148 decimal)
	opcode0_ldx5   = 0225U, // (149 decimal)
	opcode0_ldx6   = 0226U, // (150 decimal)
	opcode0_ldx7   = 0227U, // (151 decimal)
	opcode0_lbar   = 0230U, // (152 decimal)
	opcode0_rsw    = 0231U, // (153 decimal)
	opcode0_ldbr   = 0232U, // (154 decimal)
	opcode0_rmcm   = 0233U, // (155 decimal)
	opcode0_szn    = 0234U, // (156 decimal)
	opcode0_lda    = 0235U, // (157 decimal)
	opcode0_ldq    = 0236U, // (158 decimal)
	opcode0_ldaq   = 0237U, // (159 decimal)
	opcode0_orsx0  = 0240U, // (160 decimal)
	opcode0_orsx1  = 0241U, // (161 decimal)
	opcode0_orsx2  = 0242U, // (162 decimal)
	opcode0_orsx3  = 0243U, // (163 decimal)
	opcode0_orsx4  = 0244U, // (164 decimal)
	opcode0_orsx5  = 0245U, // (165 decimal)
	opcode0_orsx6  = 0246U, // (166 decimal)
	opcode0_orsx7  = 0247U, // (167 decimal)
	opcode0_spri0  = 0250U, // (168 decimal)
	opcode0_spbp1  = 0251U, // (169 decimal)
	opcode0_spri2  = 0252U, // (170 decimal)
	opcode0_spbp3  = 0253U, // (171 decimal)
	opcode0_spri   = 0254U, // (172 decimal)
	opcode0_orsa   = 0255U, // (173 decimal)
	opcode0_orsq   = 0256U, // (174 decimal)
	opcode0_lsdp   = 0257U, // (175 decimal)
	opcode0_orx0   = 0260U, // (176 decimal)
	opcode0_orx1   = 0261U, // (177 decimal)
	opcode0_orx2   = 0262U, // (178 decimal)
	opcode0_orx3   = 0263U, // (179 decimal)
	opcode0_orx4   = 0264U, // (180 decimal)
	opcode0_orx5   = 0265U, // (181 decimal)
	opcode0_orx6   = 0266U, // (182 decimal)
	opcode0_orx7   = 0267U, // (183 decimal)
	opcode0_tsp0   = 0270U, // (184 decimal)
	opcode0_tsp1   = 0271U, // (185 decimal)
	opcode0_tsp2   = 0272U, // (186 decimal)
	opcode0_tsp3   = 0273U, // (187 decimal)
	opcode0_ora    = 0275U, // (189 decimal)
	opcode0_orq    = 0276U, // (190 decimal)
	opcode0_oraq   = 0277U, // (191 decimal)
	opcode0_canx0  = 0300U, // (192 decimal)
	opcode0_canx1  = 0301U, // (193 decimal)
	opcode0_canx2  = 0302U, // (194 decimal)
	opcode0_canx3  = 0303U, // (195 decimal)
	opcode0_canx4  = 0304U, // (196 decimal)
	opcode0_canx5  = 0305U, // (197 decimal)
	opcode0_canx6  = 0306U, // (198 decimal)
	opcode0_canx7  = 0307U, // (199 decimal)
	opcode0_eawp0  = 0310U, // (200 decimal)
	opcode0_easp0  = 0311U, // (201 decimal)
	opcode0_eawp2  = 0312U, // (202 decimal)
	opcode0_easp2  = 0313U, // (203 decimal)
	opcode0_cana   = 0315U, // (205 decimal)
	opcode0_canq   = 0316U, // (206 decimal)
	opcode0_canaq  = 0317U, // (207 decimal)
	opcode0_lcx0   = 0320U, // (208 decimal)
	opcode0_lcx1   = 0321U, // (209 decimal)
	opcode0_lcx2   = 0322U, // (210 decimal)
	opcode0_lcx3   = 0323U, // (211 decimal)
	opcode0_lcx4   = 0324U, // (212 decimal)
	opcode0_lcx5   = 0325U, // (213 decimal)
	opcode0_lcx6   = 0326U, // (214 decimal)
	opcode0_lcx7   = 0327U, // (215 decimal)
	opcode0_eawp4  = 0330U, // (216 decimal)
	opcode0_easp4  = 0331U, // (217 decimal)
	opcode0_eawp6  = 0332U, // (218 decimal)
	opcode0_easp6  = 0333U, // (219 decimal)
	opcode0_lca    = 0335U, // (221 decimal)
	opcode0_lcq    = 0336U, // (222 decimal)
	opcode0_lcaq   = 0337U, // (223 decimal)
	opcode0_ansx0  = 0340U, // (224 decimal)
	opcode0_ansx1  = 0341U, // (225 decimal)
	opcode0_ansx2  = 0342U, // (226 decimal)
	opcode0_ansx3  = 0343U, // (227 decimal)
	opcode0_ansx4  = 0344U, // (228 decimal)
	opcode0_ansx5  = 0345U, // (229 decimal)
	opcode0_ansx6  = 0346U, // (230 decimal)
	opcode0_ansx7  = 0347U, // (231 decimal)
	opcode0_epp0   = 0350U, // (232 decimal)
	opcode0_epbp1  = 0351U, // (233 decimal)
	opcode0_epp2   = 0352U, // (234 decimal)
	opcode0_epbp3  = 0353U, // (235 decimal)
	opcode0_stac   = 0354U, // (236 decimal)
	opcode0_ansa   = 0355U, // (237 decimal)
	opcode0_ansq   = 0356U, // (238 decimal)
	opcode0_stcd   = 0357U, // (239 decimal)
	opcode0_anx0   = 0360U, // (240 decimal)
	opcode0_anx1   = 0361U, // (241 decimal)
	opcode0_anx2   = 0362U, // (242 decimal)
	opcode0_anx3   = 0363U, // (243 decimal)
	opcode0_anx4   = 0364U, // (244 decimal)
	opcode0_anx5   = 0365U, // (245 decimal)
	opcode0_anx6   = 0366U, // (246 decimal)
	opcode0_anx7   = 0367U, // (247 decimal)
	opcode0_epp4   = 0370U, // (248 decimal)
	opcode0_epbp5  = 0371U, // (249 decimal)
	opcode0_epp6   = 0372U, // (250 decimal)
	opcode0_epbp7  = 0373U, // (251 decimal)
	opcode0_ana    = 0375U, // (253 decimal)
	opcode0_anq    = 0376U, // (254 decimal)
	opcode0_anaq   = 0377U, // (255 decimal)
	opcode0_mpf    = 0401U, // (257 decimal)
	opcode0_mpy    = 0402U, // (258 decimal)
	opcode0_cmg    = 0405U, // (261 decimal)
	opcode0_lde    = 0411U, // (265 decimal)
	opcode0_rscr   = 0413U, // (267 decimal)
	opcode0_ade    = 0415U, // (269 decimal)
	opcode0_ufm    = 0421U, // (273 decimal)
	opcode0_dufm   = 0423U, // (275 decimal)
	opcode0_fcmg   = 0425U, // (277 decimal)
	opcode0_dfcmg  = 0427U, // (279 decimal)
	opcode0_fszn   = 0430U, // (280 decimal)
	opcode0_fld    = 0431U, // (281 decimal)
	opcode0_dfld   = 0433U, // (283 decimal)
	opcode0_ufa    = 0435U, // (285 decimal)
	opcode0_dufa   = 0437U, // (287 decimal)
	opcode0_sxl0   = 0440U, // (288 decimal)
	opcode0_sxl1   = 0441U, // (289 decimal)
	opcode0_sxl2   = 0442U, // (290 decimal)
	opcode0_sxl3   = 0443U, // (291 decimal)
	opcode0_sxl4   = 0444U, // (292 decimal)
	opcode0_sxl5   = 0445U, // (293 decimal)
	opcode0_sxl6   = 0446U, // (294 decimal)
	opcode0_sxl7   = 0447U, // (295 decimal)
	opcode0_stz    = 0450U, // (296 decimal)
	opcode0_smic   = 0451U, // (297 decimal)
	opcode0_scpr   = 0452U, // (298 decimal)
	opcode0_stt    = 0454U, // (300 decimal)
	opcode0_fst    = 0455U, // (301 decimal)
	opcode0_ste    = 0456U, // (302 decimal)
	opcode0_dfst   = 0457U, // (303 decimal)
	opcode0_fmp    = 0461U, // (305 decimal)
	opcode0_dfmp   = 0463U, // (307 decimal)
	opcode0_fstr   = 0470U, // (312 decimal)
	opcode0_frd    = 0471U, // (313 decimal)
	opcode0_dfstr  = 0472U, // (314 decimal)
	opcode0_dfrd   = 0473U, // (315 decimal)
	opcode0_fad    = 0475U, // (317 decimal)
	opcode0_dfad   = 0477U, // (319 decimal)
	opcode0_rpl    = 0500U, // (320 decimal)
	opcode0_bcd    = 0505U, // (325 decimal)
	opcode0_div    = 0506U, // (326 decimal)
	opcode0_dvf    = 0507U, // (327 decimal)
	opcode0_fneg   = 0513U, // (331 decimal)
	opcode0_fcmp   = 0515U, // (333 decimal)
	opcode0_dfcmp  = 0517U, // (335 decimal)
	opcode0_rpt    = 0520U, // (336 decimal)
	opcode0_fdi    = 0525U, // (341 decimal)
	opcode0_dfdi   = 0527U, // (343 decimal)
	opcode0_neg    = 0531U, // (345 decimal)
	opcode0_cams   = 0532U, // (346 decimal)
	opcode0_negl   = 0533U, // (347 decimal)
	opcode0_ufs    = 0535U, // (349 decimal)
	opcode0_dufs   = 0537U, // (351 decimal)
	opcode0_sprp0  = 0540U, // (352 decimal)
	opcode0_sprp1  = 0541U, // (353 decimal)
	opcode0_sprp2  = 0542U, // (354 decimal)
	opcode0_sprp3  = 0543U, // (355 decimal)
	opcode0_sprp4  = 0544U, // (356 decimal)
	opcode0_sprp5  = 0545U, // (357 decimal)
	opcode0_sprp6  = 0546U, // (358 decimal)
	opcode0_sprp7  = 0547U, // (359 decimal)
	opcode0_sbar   = 0550U, // (360 decimal)
	opcode0_stba   = 0551U, // (361 decimal)
	opcode0_stbq   = 0552U, // (362 decimal)
	opcode0_smcm   = 0553U, // (363 decimal)
	opcode0_stc1   = 0554U, // (364 decimal)
	opcode0_ssdp   = 0557U, // (367 decimal)
	opcode0_rpd    = 0560U, // (368 decimal)
	opcode0_fdv    = 0565U, // (373 decimal)
	opcode0_dfdv   = 0567U, // (375 decimal)
	opcode0_fno    = 0573U, // (379 decimal)
	opcode0_fsb    = 0575U, // (381 decimal)
	opcode0_dfsb   = 0577U, // (383 decimal)
	opcode0_tze    = 0600U, // (384 decimal)
	opcode0_tnz    = 0601U, // (385 decimal)
	opcode0_tnc    = 0602U, // (386 decimal)
	opcode0_trc    = 0603U, // (387 decimal)
	opcode0_tmi    = 0604U, // (388 decimal)
	opcode0_tpl    = 0605U, // (389 decimal)
	opcode0_ttf    = 0607U, // (391 decimal)
	opcode0_rtcd   = 0610U, // (392 decimal)
	opcode0_rcu    = 0613U, // (395 decimal)
	opcode0_teo    = 0614U, // (396 decimal)
	opcode0_teu    = 0615U, // (397 decimal)
	opcode0_dis    = 0616U, // (398 decimal)
	opcode0_tov    = 0617U, // (399 decimal)
	opcode0_eax0   = 0620U, // (400 decimal)
	opcode0_eax1   = 0621U, // (401 decimal)
	opcode0_eax2   = 0622U, // (402 decimal)
	opcode0_eax3   = 0623U, // (403 decimal)
	opcode0_eax4   = 0624U, // (404 decimal)
	opcode0_eax5   = 0625U, // (405 decimal)
	opcode0_eax6   = 0626U, // (406 decimal)
	opcode0_eax7   = 0627U, // (407 decimal)
	opcode0_ret    = 0630U, // (408 decimal)
	opcode0_rccl   = 0633U, // (411 decimal)
	opcode0_ldi    = 0634U, // (412 decimal)
	opcode0_eaa    = 0635U, // (413 decimal)
	opcode0_eaq    = 0636U, // (414 decimal)
	opcode0_ldt    = 0637U, // (415 decimal)
	opcode0_ersx0  = 0640U, // (416 decimal)
	opcode0_ersx1  = 0641U, // (417 decimal)
	opcode0_ersx2  = 0642U, // (418 decimal)
	opcode0_ersx3  = 0643U, // (419 decimal)
	opcode0_ersx4  = 0644U, // (420 decimal)
	opcode0_ersx5  = 0645U, // (421 decimal)
	opcode0_ersx6  = 0646U, // (422 decimal)
	opcode0_ersx7  = 0647U, // (423 decimal)
	opcode0_spri4  = 0650U, // (424 decimal)
	opcode0_spbp5  = 0651U, // (425 decimal)
	opcode0_spri6  = 0652U, // (426 decimal)
	opcode0_spbp7  = 0653U, // (427 decimal)
	opcode0_stacq  = 0654U, // (428 decimal)
	opcode0_ersa   = 0655U, // (429 decimal)
	opcode0_ersq   = 0656U, // (430 decimal)
	opcode0_scu    = 0657U, // (431 decimal)
	opcode0_erx0   = 0660U, // (432 decimal)
	opcode0_erx1   = 0661U, // (433 decimal)
	opcode0_erx2   = 0662U, // (434 decimal)
	opcode0_erx3   = 0663U, // (435 decimal)
	opcode0_erx4   = 0664U, // (436 decimal)
	opcode0_erx5   = 0665U, // (437 decimal)
	opcode0_erx6   = 0666U, // (438 decimal)
	opcode0_erx7   = 0667U, // (439 decimal)
	opcode0_tsp4   = 0670U, // (440 decimal)
	opcode0_tsp5   = 0671U, // (441 decimal)
	opcode0_tsp6   = 0672U, // (442 decimal)
	opcode0_tsp7   = 0673U, // (443 decimal)
	opcode0_lcpr   = 0674U, // (444 decimal)
	opcode0_era    = 0675U, // (445 decimal)
	opcode0_erq    = 0676U, // (446 decimal)
	opcode0_eraq   = 0677U, // (447 decimal)
	opcode0_tsx0   = 0700U, // (448 decimal)
	opcode0_tsx1   = 0701U, // (449 decimal)
	opcode0_tsx2   = 0702U, // (450 decimal)
	opcode0_tsx3   = 0703U, // (451 decimal)
	opcode0_tsx4   = 0704U, // (452 decimal)
	opcode0_tsx5   = 0705U, // (453 decimal)
	opcode0_tsx6   = 0706U, // (454 decimal)
	opcode0_tsx7   = 0707U, // (455 decimal)
	opcode0_tra    = 0710U, // (456 decimal)
	opcode0_call6  = 0713U, // (459 decimal)
	opcode0_tss    = 0715U, // (461 decimal)
	opcode0_xec    = 0716U, // (462 decimal)
	opcode0_xed    = 0717U, // (463 decimal)
	opcode0_lxl0   = 0720U, // (464 decimal)
	opcode0_lxl1   = 0721U, // (465 decimal)
	opcode0_lxl2   = 0722U, // (466 decimal)
	opcode0_lxl3   = 0723U, // (467 decimal)
	opcode0_lxl4   = 0724U, // (468 decimal)
	opcode0_lxl5   = 0725U, // (469 decimal)
	opcode0_lxl6   = 0726U, // (470 decimal)
	opcode0_lxl7   = 0727U, // (471 decimal)
	opcode0_ars    = 0731U, // (473 decimal)
	opcode0_qrs    = 0732U, // (474 decimal)
	opcode0_lrs    = 0733U, // (475 decimal)
	opcode0_als    = 0735U, // (477 decimal)
	opcode0_qls    = 0736U, // (478 decimal)
	opcode0_lls    = 0737U, // (479 decimal)
	opcode0_stx0   = 0740U, // (480 decimal)
	opcode0_stx1   = 0741U, // (481 decimal)
	opcode0_stx2   = 0742U, // (482 decimal)
	opcode0_stx3   = 0743U, // (483 decimal)
	opcode0_stx4   = 0744U, // (484 decimal)
	opcode0_stx5   = 0745U, // (485 decimal)
	opcode0_stx6   = 0746U, // (486 decimal)
	opcode0_stx7   = 0747U, // (487 decimal)
	opcode0_stc2   = 0750U, // (488 decimal)
	opcode0_stca   = 0751U, // (489 decimal)
	opcode0_stcq   = 0752U, // (490 decimal)
	opcode0_sreg   = 0753U, // (491 decimal)
	opcode0_sti    = 0754U, // (492 decimal)
	opcode0_sta    = 0755U, // (493 decimal)
	opcode0_stq    = 0756U, // (494 decimal)
	opcode0_staq   = 0757U, // (495 decimal)
	opcode0_lprp0  = 0760U, // (496 decimal)
	opcode0_lprp1  = 0761U, // (497 decimal)
	opcode0_lprp2  = 0762U, // (498 decimal)
	opcode0_lprp3  = 0763U, // (499 decimal)
	opcode0_lprp4  = 0764U, // (500 decimal)
	opcode0_lprp5  = 0765U, // (501 decimal)
	opcode0_lprp6  = 0766U, // (502 decimal)
	opcode0_lprp7  = 0767U, // (503 decimal)
	opcode0_arl    = 0771U, // (505 decimal)
	opcode0_qrl    = 0772U, // (506 decimal)
	opcode0_lrl    = 0773U, // (507 decimal)
	opcode0_gtb    = 0774U, // (508 decimal)
	opcode0_alr    = 0775U, // (509 decimal)
	opcode0_qlr    = 0776U, // (510 decimal)
	opcode0_llr    = 0777U  // (511 decimal)
} opcode0_t;

// Opcodes with low bit (bit 27) == 1.  Enum value is value of upper 9 bits.
typedef enum {
	opcode1_mve    = 0020U, // (16 decimal)
	opcode1_mvne   = 0024U, // (20 decimal)
	opcode1_csl    = 0060U, // (48 decimal)
	opcode1_csr    = 0061U, // (49 decimal)
	opcode1_sztl   = 0064U, // (52 decimal)
	opcode1_sztr   = 0065U, // (53 decimal)
	opcode1_cmpb   = 0066U, // (54 decimal)
	opcode1_mlr    = 0100U, // (64 decimal)
	opcode1_mrl    = 0101U, // (65 decimal)
	opcode1_cmpc   = 0106U, // (70 decimal)
	opcode1_scd    = 0120U, // (80 decimal)
	opcode1_scdr   = 0121U, // (81 decimal)
	opcode1_scm    = 0124U, // (84 decimal)
	opcode1_scmr   = 0125U, // (85 decimal)
	opcode1_sptr   = 0154U, // (108 decimal)
	opcode1_mvt    = 0160U, // (112 decimal)
	opcode1_tct    = 0164U, // (116 decimal)
	opcode1_tctr   = 0165U, // (117 decimal)
	opcode1_lptr   = 0173U, // (123 decimal)
	opcode1_ad2d   = 0202U, // (130 decimal)
	opcode1_sb2d   = 0203U, // (131 decimal)
	opcode1_mp2d   = 0206U, // (134 decimal)
	opcode1_dv2d   = 0207U, // (135 decimal)
	opcode1_ad3d   = 0222U, // (146 decimal)
	opcode1_sb3d   = 0223U, // (147 decimal)
	opcode1_mp3d   = 0226U, // (150 decimal)
	opcode1_dv3d   = 0227U, // (151 decimal)
	opcode1_lsdr   = 0232U, // (154 decimal)
	opcode1_spbp0  = 0250U, // (168 decimal)
	opcode1_spri1  = 0251U, // (169 decimal)
	opcode1_spbp2  = 0252U, // (170 decimal)
	opcode1_spri3  = 0253U, // (171 decimal)
	opcode1_ssdr   = 0254U, // (172 decimal)
	opcode1_lptp   = 0257U, // (175 decimal)
	opcode1_mvn    = 0300U, // (192 decimal)
	opcode1_btd    = 0301U, // (193 decimal)
	opcode1_cmpn   = 0303U, // (195 decimal)
	opcode1_dtb    = 0305U, // (197 decimal)
	opcode1_easp1  = 0310U, // (200 decimal)
	opcode1_eawp1  = 0311U, // (201 decimal)
	opcode1_easp3  = 0312U, // (202 decimal)
	opcode1_eawp3  = 0313U, // (203 decimal)
	opcode1_easp5  = 0330U, // (216 decimal)
	opcode1_eawp5  = 0331U, // (217 decimal)
	opcode1_easp7  = 0332U, // (218 decimal)
	opcode1_eawp7  = 0333U, // (219 decimal)
	opcode1_epbp0  = 0350U, // (232 decimal)
	opcode1_epp1   = 0351U, // (233 decimal)
	opcode1_epbp2  = 0352U, // (234 decimal)
	opcode1_epp3   = 0353U, // (235 decimal)
	opcode1_epbp4  = 0370U, // (248 decimal)
	opcode1_epp5   = 0371U, // (249 decimal)
	opcode1_epbp6  = 0372U, // (250 decimal)
	opcode1_epp7   = 0373U, // (251 decimal)
	opcode1_sareg  = 0443U, // (291 decimal)
	opcode1_spl    = 0447U, // (295 decimal)
	opcode1_lareg  = 0463U, // (307 decimal)
	opcode1_lpl    = 0467U, // (311 decimal)
	opcode1_a9bd   = 0500U, // (320 decimal)
	opcode1_a6bd   = 0501U, // (321 decimal)
	opcode1_a4bd   = 0502U, // (322 decimal)
	opcode1_abd    = 0503U, // (323 decimal)
	opcode1_awd    = 0507U, // (327 decimal)
	opcode1_s9bd   = 0520U, // (336 decimal)
	opcode1_s6bd   = 0521U, // (337 decimal)
	opcode1_s4bd   = 0522U, // (338 decimal)
	opcode1_sbd    = 0523U, // (339 decimal)
	opcode1_swd    = 0527U, // (343 decimal)
	opcode1_camp   = 0532U, // (346 decimal)
	opcode1_ara0   = 0540U, // (352 decimal)
	opcode1_ara1   = 0541U, // (353 decimal)
	opcode1_ara2   = 0542U, // (354 decimal)
	opcode1_ara3   = 0543U, // (355 decimal)
	opcode1_ara4   = 0544U, // (356 decimal)
	opcode1_ara5   = 0545U, // (357 decimal)
	opcode1_ara6   = 0546U, // (358 decimal)
	opcode1_ara7   = 0547U, // (359 decimal)
	opcode1_sptp   = 0557U, // (367 decimal)
	opcode1_aar0   = 0560U, // (368 decimal)
	opcode1_aar1   = 0561U, // (369 decimal)
	opcode1_aar2   = 0562U, // (370 decimal)
	opcode1_aar3   = 0563U, // (371 decimal)
	opcode1_aar4   = 0564U, // (372 decimal)
	opcode1_aar5   = 0565U, // (373 decimal)
	opcode1_aar6   = 0566U, // (374 decimal)
	opcode1_aar7   = 0567U, // (375 decimal)
	opcode1_trtn   = 0600U, // (384 decimal)
	opcode1_trtf   = 0601U, // (385 decimal)
	opcode1_tmoz   = 0604U, // (388 decimal)
	opcode1_tpnz   = 0605U, // (389 decimal)
	opcode1_ttn    = 0606U, // (390 decimal)
	opcode1_arn0   = 0640U, // (416 decimal)
	opcode1_arn1   = 0641U, // (417 decimal)
	opcode1_arn2   = 0642U, // (418 decimal)
	opcode1_arn3   = 0643U, // (419 decimal)
	opcode1_arn4   = 0644U, // (420 decimal)
	opcode1_arn5   = 0645U, // (421 decimal)
	opcode1_arn6   = 0646U, // (422 decimal)
	opcode1_arn7   = 0647U, // (423 decimal)
	opcode1_spbp4  = 0650U, // (424 decimal)
	opcode1_spri5  = 0651U, // (425 decimal)
	opcode1_spbp6  = 0652U, // (426 decimal)
	opcode1_spri7  = 0653U, // (427 decimal)
	opcode1_nar0   = 0660U, // (432 decimal)
	opcode1_nar1   = 0661U, // (433 decimal)
	opcode1_nar2   = 0662U, // (434 decimal)
	opcode1_nar3   = 0663U, // (435 decimal)
	opcode1_nar4   = 0664U, // (436 decimal)
	opcode1_nar5   = 0665U, // (437 decimal)
	opcode1_nar6   = 0666U, // (438 decimal)
	opcode1_nar7   = 0667U, // (439 decimal)
	opcode1_sar0   = 0740U, // (480 decimal)
	opcode1_sar1   = 0741U, // (481 decimal)
	opcode1_sar2   = 0742U, // (482 decimal)
	opcode1_sar3   = 0743U, // (483 decimal)
	opcode1_sar4   = 0744U, // (484 decimal)
	opcode1_sar5   = 0745U, // (485 decimal)
	opcode1_sar6   = 0746U, // (486 decimal)
	opcode1_sar7   = 0747U, // (487 decimal)
	opcode1_sra    = 0754U, // (492 decimal)
	opcode1_lar0   = 0760U, // (496 decimal)
	opcode1_lar1   = 0761U, // (497 decimal)
	opcode1_lar2   = 0762U, // (498 decimal)
	opcode1_lar3   = 0763U, // (499 decimal)
	opcode1_lar4   = 0764U, // (500 decimal)
	opcode1_lar5   = 0765U, // (501 decimal)
	opcode1_lar6   = 0766U, // (502 decimal)
	opcode1_lar7   = 0767U, // (503 decimal)
	opcode1_lra    = 0774U  // (508 decimal)
} opcode1_t;

// The CPU supports 3 addressing modes
// [CAC] I tell a lie: 4 modes....
typedef enum { ABSOLUTE_mode = ABSOLUTE_MODE, APPEND_mode = APPEND_MODE, BAR_mode = BAR_MODE } addr_modes_t;


// The control unit of the CPU is always in one of several states. We
// don't currently use all of the states used in the physical CPU.
// The FAULT_EXEC cycle did not exist in the physical hardware.
typedef enum {
    ABORT_cycle /* = ABORT_CYCLE */,
    FAULT_cycle /* = FAULT_CYCLE */,
    EXEC_cycle,
    FAULT_EXEC_cycle,
    FAULT_EXEC2_cycle,
    INTERRUPT_cycle,
    INTERRUPT_EXEC_cycle,
    INTERRUPT_EXEC2_cycle,
    FETCH_cycle = INSTRUCTION_FETCH
    // CA FETCH OPSTORE, DIVIDE_EXEC
} cycles_t;

/* MF fields of EIS multi-word instructions -- 7 bits */
typedef struct {
    bool ar;
    bool rl;
    bool id;
    uint reg;  // 4 bits
} eis_mf_t;

/* Format of a 36 bit instruction word */
typedef struct {
    uint addr;    // 18 bits at  0..17; 18 bit offset or seg/offset pair
    uint opcode;  // 10 bits at 18..27
    uint inhibit; //  1 bit  at 28
    union {
        struct {
            uint pr_bit;  // 1 bit at 29; use offset[0..2] as pointer reg
            uint tag;     // 6 bits at 30..35 */
        } single;
        eis_mf_t mf1;     // from bits 29..35 of EIS instructions
    } mods;
    bool is_eis_multiword;  // set true for relevent opcodes
    
    t_uint64 *wordEven; // HWR
    
} instr_t;



extern int xec_side_effect;

// h6180 stuff
/* [map] designates mapping into 36-bit word from DPS-8 proc manual */

/* GE-625/635 */

extern word36	rA;	/*!< accumulator */
extern word36	rQ;	/*!< quotient */
extern word8	rE;	/*!< exponent [map: rE, 28 0's] */

extern word18	rX[8];	/*!< index */
extern word27	rTR;	/*!< timer [map: TR, 9 0's] */
extern word24	rY;     /*!< address operand */
extern word8	rTAG;	/*!< instruction tag */
extern word8	tTB;	/*!< char size indicator (TB6=6-bit,TB9=9-bit) [3b] */
extern word8	tCF;	/*!< character position field [3b] */
extern word8	rRALR;	/*!< ring alarm [3b] [map: 33 0's, RALR] */
extern struct _tpr {
    word3   TRR; ///< The current effective ring number
    word15  TSR; ///< The current effective segment number
    word6   TBR; ///< The current bit offset as calculated from ITS and ITP pointer pairs.
    word18  CA;  ///< The current computed address relative to the origin of the segment whose segment number is in TPR.TSR
} TPR;

extern struct _ppr {
    word3   PRR; ///< The number of the ring in which the process is executing. It is set to the effective ring number of the procedure segment when control is transferred to the procedure.
    word15  PSR; ///< The segment number of the procedure being executed.
    word1   P;  ///< A flag controlling execution of privileged instructions. Its value is 1 (permitting execution of privileged instructions) if PPR.PRR is 0 and the privileged bit in the segment descriptor word (SDW.P) for the procedure is 1; otherwise, its value is 0.
    word18  IC;  ///< The word offset from the origin of the procedure segment to the current instruction. (same as PPR.IC)
} PPR;

/////

/*!
 The terms "pointer register" and "address register" both apply to the same physical hardware. The distinction arises from the manner in which the register is used and in the interpretation of the register contents. "Pointer register" refers to the register as used by the appending unit and "address register" refers to the register as used by the decimal unit.
 The three forms are compatible and may be freely intermixed. For example, PRn may be loaded in pointer register form with the Effective Pointer to Pointer Register n (eppn) instruction, then modified in pointer register form with the Effective Address to Word/Bit Number of Pointer Register n (eawpn) instruction, then further modified in address register form (assuming character size k) with the Add k-Bit Displacement to Address Register (akbd) instruction, and finally invoked in operand descriptor form by the use of MF.AR in an EIS multiword instruction .
 The reader's attention is directed to the presence of two bit number registers, PRn.BITNO and ARn.BITNO. Because the Multics processor was implemented as an enhancement to an existing design, certain apparent anomalies appear. One of these is the difference in the handling of unaligned data items by the appending unit and decimal unit. The decimal unit handles all unaligned data items with a 9-bit byte number and bit offset within the byte. Conversion from the description given in the EIS operand descriptor is done automatically by the hardware. The appending unit maintains compatibility with the earlier generation Multics processor by handling all unaligned data items with a bit offset from the prior word boundary; again with any necessary conversion done automatically by the hardware. Thus, a pointer register, PRn, may be loaded from an ITS pointer pair having a pure bit offset and modified by one of the EIS address register instructions (a4bd, s9bd, etc.) using character displacement counts. The automatic conversion performed ensures that the pointer register, PRi, and its matching address register, ARi, both describe the same physical bit in main memory.
 
     XXX Subtle differences between the interpretation of PR/AR. Need to take this into account.
 
     * For Pointer Registers:
       - PRn.WORDNO The offset in words from the base or origin of the segment to the data item.
       - PRn.BITNO The number of the bit within PRn.WORDNO that is the first bit of the data item. Data items aligned on word boundaries always have the value 0. Unaligned data items may have any value in the range [1,35].
 
     * For Address Registers:
       - ARn.WORDNO The offset in words relative to the current addressing base referent (segment origin, BAR.BASE, or absolute 0 depending on addressing mode) to the word containing the next data item element.
       - ARn.CHAR The number of the 9-bit byte within ARn.WORDNO containing the first bit of the next data item element.
       - ARn.BITNO The number of the bit within ARn.CHAR that is the first bit of the next data item element.
 */
extern struct _par {
    word15  SNR;    ///< The segment number of the segment containing the data item described by the pointer register.
    word3   RNR;    ///< The final effective ring number value calculated during execution of the instruction that last loaded the PR.
#if 0
    word6   BITNO;  ///< The number of the bit within PRn.WORDNO that is the first bit of the data item. Data items aligned on word boundaries always have the value 0. Unaligned data items may have any value in the range [1,35].
    word18  WORDNO; ///< The offset in words from the base or origin of the segment to the data item.
                        // The offset in words relative to the current addressing base referent (segment origin, BAR.BASE, or absolute 0 depending on addressing mode) to the word containing the next data item element.
    word2   CHAR;   ///< The number of the 9-bit byte within ARn.WORDNO containing the first bit of the next data item element.
#elseif 0
    word18  WORDNO; ///< The offset in words from the base or origin of the segment to the data item.
    union {
      struct {
        word2 CHAR:2;
        word4 ABITNO:4;
      };
      word6 PBITNO:6;
    };
#else
    // To get the correct behavior, the ARn.BITNO and .CHAR need to be kept in
    // sync. BITNO is the canonical value; access routines for AR[n].BITNO and 
    // .CHAR are provided
    word6   BITNO;  ///< The number of the bit within PRn.WORDNO that is the first bit of the data item. Data items aligned on word boundaries always have the value 0. Unaligned data items may have any value in the range [1,35].
    word18  WORDNO; ///< The offset in words from the base or origin of the segment to the data item.

#endif
} PAR[8];

#define AR    PAR   // XXX remember there are subtle differences between AR/PR.BITNO
#define PR    PAR

// Support code to access ARn.BITNO and CHAR

#define GET_AR_BITNO(n) (PAR[n].BITNO % 9)
#define GET_AR_CHAR(n) (PAR[n].BITNO / 9)
#define SET_AR_BITNO(n, b) PAR[n].BITNO = (GET_AR_CHAR[n] * 9 + ((b) & 017))
#define SET_AR_CHAR(n, c) PAR[n].BITNO = (GET_AR_BITNO[n] + ((c) & 03) * 9)
#define SET_AR_CHAR_BIT(n, c, b) PAR[n].BITNO = (((c) & 03) * 9 + ((b) & 017))

extern struct _bar {
    word9 BASE;     ///< Contains the 9 high-order bits of an 18-bit address relocation constant. The low-order bits are generated as zeros.
    word9 BOUND;    ///< Contains the 9 high-order bits of the unrelocated address limit. The low- order bits are generated as zeros. An attempt to access main memory beyond this limit causes a store fault, out of bounds. A value of 0 is truly 0, indicating a null memory range.
} BAR;

extern struct _dsbr {
    word24  ADDR;   ///< If DSBR.U = 1, the 24-bit absolute main memory address of the origin of the current descriptor segment; otherwise, the 24-bit absolute main memory address of the page table for the current descriptor segment.
    word14  BND;    ///< The 14 most significant bits of the highest Y-block16 address of the descriptor segment that can be addressed without causing an access violation, out of segment bounds, fault.
    word1   U;      ///< A flag specifying whether the descriptor segment is unpaged (U = 1) or paged (U = 0).
    word12  STACK;  ///< The upper 12 bits of the 15-bit stack base segment number. It is used only during the execution of the call6 instruction. (See Section 8 for a discussion of generation of the stack segment number.)
} DSBR;

//! The segment descriptor word (SDW) pair contains information that controls the access to a segment. The SDW for segment n is located at offset 2n in the descriptor segment whose description is currently loaded into the descriptor segment base register (DSBR).
extern struct _sdw {  ///< as used by APU
    word24  ADDR;   ///< The 24-bit absolute main memory address of the page table for the target segment if SDWAM.U = 0; otherwise, the 24-bit absolute main memory address of the origin of the target segment.
    word3   R1; ///< Upper limit of read/write ring bracket
    word3   R2; ///< Upper limit of read/execute ring bracket
    word3   R3; ///< Upper limit of call ring bracket
    word14  BOUND;  ///< The 14 high-order bits of the last Y-block16 address within the segment that can be referenced without an access violation, out of segment bound, fault.
    word1   R;  ///< Read permission bit. If this bit is set ON, read access requests are allowed.
    word1   E;  ///< Execute permission bit. If this bit is set ON, the SDW may be loaded into the procedure pointer register (PPR) and instructions fetched from the segment for execution.
    word1   W;  ///< Write permission bit. If this bit is set ON, write access requests are allowed.
    word1   P;  ///< Privileged flag bit. If this bit is set ON, privileged instructions from the segment may be executed if PPR.PRR is 0.
    word1   U;  ///< Unpaged flag bit. If this bit is set ON, the segment is unpaged and SDWAM.ADDR is the 24-bit absolute main memory address of the origin of the segment. If this bit is set OFF, the segment is paged and SDWAM.ADDR is the 24-bit absolute main memory address of the page table for the segment.
    word1   G;  ///< Gate control bit. If this bit is set OFF, calls and transfers into the segment must be to an offset no greater than the value of SDWAM.CL as described below.
    word1   C;  ///< Cache control bit. If this bit is set ON, data and/or instructions from the segment may be placed in the cache memory.
    word14  CL; ///< Call limiter (entry bound) value. If SDWAM.G is set OFF, transfers of control into the segment must be to segment addresses no greater than this value.
    word15  POINTER;    ///< The effective segment number used to fetch this SDW from main memory.
    word1   F;          ///< Full/empty bit. If this bit is set ON, the SDW in the register is valid. If this bit is set OFF, a hit is not possible. All SDWAM.F bits are set OFF by the instructions that clear the SDWAM.
    word6   USE;        ///< Usage count for the register. The SDWAM.USE field is used to maintain a strict FIFO queue order among the SDWs. When an SDW is matched, its USE value is set to 15 (newest) on the DPS/L68 and to 63 on the DPS 8M, and the queue is reordered. SDWs newly fetched from main memory replace the SDW with USE value 0 (oldest) and the queue is reordered.
    
    //bool    _initialized; ///< for emulator use. When true SDWAM entry has been initialized/used ...
    
} SDWAM[64], *SDW;
typedef struct _sdw _sdw;

//* in-core SDW (i.e. not cached, or in SDWAM)
extern struct _sdw0 {
    // even word
    word24  ADDR;   ///< The 24-bit absolute main memory address of the page table for the target segment if SDWAM.U = 0; otherwise, the 24-bit absolute main memory address of the origin of the target segment.
    word3   R1;     ///< Upper limit of read/write ring bracket
    word3   R2;     ///< Upper limit of read/execute ring bracket
    word3   R3;     ///< Upper limit of call ring bracket
    word1   F;      ///< Directed fault flag
                    // * 0 = page not in main memory; execute directed fault FC
                    // * 1 = page is in main memory
    word2   FC;     ///< directed fault number for page fault.
    
    // odd word
    word14  BOUND;  ///< The 14 high-order bits of the last Y-block16 address within the segment that can be referenced without an access violation, out of segment bound, fault.
    word1   R;  ///< Read permission bit. If this bit is set ON, read access requests are allowed.
    word1   E;  ///< Execute permission bit. If this bit is set ON, the SDW may be loaded into the procedure pointer register (PPR) and instructions fetched from the segment for execution.
    word1   W;  ///< Write permission bit. If this bit is set ON, write access requests are allowed.
    word1   P;  ///< Privileged flag bit. If this bit is set ON, privileged instructions from the segment may be executed if PPR.PRR is 0.
    word1   U;  ///< Unpaged flag bit. If this bit is set ON, the segment is unpaged and SDWAM.ADDR is the 24-bit absolute main memory address of the origin of the segment. If this bit is set OFF, the segment is paged and SDWAM.ADDR is the 24-bit absolute main memory address of the page table for the segment.
    word1   G;  ///< Gate control bit. If this bit is set OFF, calls and transfers into the segment must be to an offset no greater than the value of SDWAM.CL as described below.
    word1   C;  ///< Cache control bit. If this bit is set ON, data and/or instructions from the segment may be placed in the cache memory.
    word14  EB; ///< Entry bound. Any call into this segment must be to an offset less than EB if G=0
} SDW0;
typedef struct _sdw0 _sdw0;


//! ptw as used by APU
extern struct _ptw {
    word18  ADDR;   ///< The 18 high-order bits of the 24-bit absolute main memory address of the page.
    word1   M;      ///< Page modified flag bit. This bit is set ON whenever the PTW is used for a store type instruction. When the bit changes value from 0 to 1, a special extra cycle is generated to write it back into the PTW in the page table in main memory.
    word15  POINTER;///< The effective segment number used to fetch this PTW from main memory.
    word12  PAGENO; ///< The 12 high-order bits of the 18-bit computed address (TPR.CA) used to fetch this PTW from main memory.
    word1   F;      ///< Full/empty bit. If this bit is set ON, the PTW in the register is valid. If this bit is set OFF, a hit is not possible. All PTWAM.F bits are set OFF by the instructions that clear the PTWAM.
    word6   USE;    ///< Usage count for the register. The PTWAM.USE field is used to maintain a strict FIFO queue order among the PTWs. When an PTW is matched its USE value is set to 15 (newest) on the DPS/L68 and to 63 on the DPS 8M, and the queue is reordered. PTWs newly fetched from main memory replace the PTW with USE value 0 (oldest) and the queue is reordered.
    
    //bool    _initialized; ///< for emulator use. When true PTWAM entry has been initialized/used ...
    
} PTWAM[64], *PTW;
typedef struct _ptw _ptw;

//! in-core PTW
extern struct _ptw0 {
    word18  ADDR;   ///< The 18 high-order bits of the 24-bit absolute main memory address of the page.
    word1   U;      // * 1 = page has been used (referenced)
    word1   M;      // * 1 = page has been modified
    word1   F;      ///< Directed fault flag
                    // * 0 = page not in main memory; execute directed fault FC
                    // * 1 = page is in main memory
    word2   FC;     ///< directed fault number for page fault.
    
} PTW0;
typedef struct _ptw0 _ptw0;

/*
 * Cache Mode Regsiter
 *
 * (dont know where else to put this)
 */

struct _cache_mode_register
{
    word15     cache_dir_address;  // Probably not used by simulator
    word1    par_bit;            // "
    word1    lev_ful;
    word1    csh1_on;
    word1    csh2_on;
    word1    opnd_on;
    word1    inst_on; // DPS8, but not DPS8M
    word1    csh_reg;
    word1    str_asd;
    word1    col_ful;
    word2     rro_AB;
    word2     luf;        // LUF value
                        // 0   1   2   3
                        // Lockup time
                        // 2ms 4ms 8ms 16ms
                        // The lockup timer is set to 16ms when the processor is initialized.
};
typedef struct _cache_mode_register _cache_mode_register;
extern _cache_mode_register CMR;

typedef struct mode_registr
  {
    word1 cuolin;
    word1 solin;
    word1 sdpap;
    word1 separ;
    word2 tm;
    word2 vm;
    word1 hrhlt;
    word1 hrxfr;
    word1 ihr;
    word1 ihrrs;
    word1 mrgctl;
    word1 hexfp;
    word1 emr;
  } _mode_register;
extern _mode_register MR;

extern DEVICE cpu_dev;
extern jmp_buf jmpMain;     ///< This is where we should return to from a fault to retry an instruction

typedef struct MOPstruct MOPstruct;

struct EISstruct
{
    DCDstruct *ins;    ///< instruction ins from whence we came
    
    word36  op0;    ///< 1st instruction word

    word36  op[3];   ///< raw operand descriptors
#define OP1 op[0]   ///< 1st descriptor (2nd ins word)
#define OP2 op[1]   ///< 2nd descriptor (3rd ins word)
#define OP3 op[2]   ///< 3rd descriptor (4th ins word)
    
    bool    P;      ///< 4-bit data sign character control
    bool    T;      ///< Truncation fault enable bit
    bool    F;      ///< for CMPB etc fill bit
    bool    R;      ///> Round enable bit
    
    int     BOLR;   ///< Boolean result control field
    
    int     MF[3];
#define MF1     MF[0]   ///< Modification field for operand descriptor 1
#define MF2     MF[1]   ///< Modification field for operand descriptor 2
#define MF3     MF[2]   ///< Modification field for operand descriptor 3

    word18  YChar9[3];
#define YChar91 YChar9[0]
#define YChar92 YChar9[1]
#define YChar93 YChar9[2]
    word18  YChar6[3];
#define YChar61 YChar6[0]
#define YChar62 YChar6[1]
#define YChar63 YChar6[2]
    word18  YChar4[3];
#define YChar41 YChar4[0]
#define YChar42 YChar4[1]
#define YChar43 YChar4[2]

    word18  YBit[3];
#define YBit1 YBit[0]
#define YBit2 YBit[1]
#define YBit3 YBit[2]

    int    CN[3];
#define CN1 CN[0]
#define CN2 CN[1]
#define CN3 CN[2]

    int    C[3];
#define C1 C[0]
#define C2 C[1]
#define C3 C[2]

    int    B[3];
#define B1 B[0]
#define B2 B[1]
#define B3 B[2]

    int     N[3];
#define N1  N[0]
#define N2  N[1]
#define N3  N[2]
    
//    word18  YCharn[3];
//#define YCharn1 YCharn[0]
//#define YCharn2 YCharn[1]
//#define YCharn3 YCharn[3]

    int    TN[3];     ///< type numeric
#define TN1 TN[0]
#define TN2 TN[1]
#define TN3 TN[2]

    int    TA[3];  ///< type alphanumeric
#define TA1 TA[0]
#define TA2 TA[1]
#define TA3 TA[2]

    int    S[3];   ///< Sign and decimal type of number
#define S1  S[0]
#define S2  S[1]
#define S3  S[2]

    int    SF[3];  ///< scale factor
#define SF1 SF[0]
#define SF2 SF[1]
#define SF3 SF[2]

    //! filled in after MF1/2/3 are parsed and examined.
//    int     L[3];
//#define L1  L[0]    ///< length of MF1 operand
//#define L2  L[1]    ///< length of MF2 operand
//#define L3  L[2]    ///< length of MF3 operand
    
    // XXX not certain what to do with these just yet ...
    int32 effBITNO;
    int32 effCHAR;
    int32 effWORDNO;
    
    word18 _flags;  ///< flags set during operation
    word18 _faults; ///< faults generated by instruction
    
    word72s x, y, z;///< a signed, 128-bit integers for playing with .....
 
    char    buff[2560]; ///< a buffer to play with (should be big enough for most of our needs)
    char    *p, *q;    ///< pointers to use/abuse for EIS
    
    
    // Stuff for Micro-operations and Edit instructions...
    
    word9   editInsertionTable[8];     // 8 9-bit chars
    
    int     mop9;       ///< current micro-operation (entire)
    int     mop;        ///< current micro-operation 5-bit code
    int     mopIF;      ///< current mocri-operation IF field
    MOPstruct *m;   ///< pointer to current MOP struct
    
    word9   inBuffer[64]; ///< decimal unit input buffer
    word9   *in;          ///< pointer to current read position in inBuffer
    word9   outBuffer[64];///< output buffer
    word9   *out;         ///< pointer to current write position in outBuffer;
    int     outPos;       ///< current character posn on output buffer word
    
    int     exponent;   ///< For decimal floating-point (evil)
    int     sign;       ///< For signed decimal (1, -1)
    
    EISaddr *mopAddress;   ///< mopAddress, pointer to addr[0], [1], or [2]
    
    //word18  mopAddr;    ///< address of micro-operations
    int     mopTally;   ///< number of micro-ops
    int     mopCN;      ///< starting at char pos CN
    
    int     mopPos;     ///< current mop char posn
    
    // Edit Flags
    // The processor provides the following four edit flags for use by the micro operations.
    
    bool    mopES; // End Suppression flag; initially OFF, set ON by a micro operation when zero-suppression ends.
    bool    mopSN; // Sign flag; initially set OFF if the sending string has an alphanumeric descriptor or an unsigned numeric descriptor. If the sending string has a signed numeric descriptor, the sign is initially read from the sending string from the digit position defined by the sign and the decimal type field (S); SN is set OFF if positive, ON if negative. If all digits are zero, the data is assumed positive and the SN flag is set OFF, even when the sign is negative.
    bool    mopZ;  // Zero flag; initially set ON and set OFF whenever a sending string character that is not decimal zero is moved into the receiving string.
    bool    mopBZ; // Blank-when-zero flag; initially set OFF and set ON by either the ENF or SES micro operation. If, at the completion of a move (L1 exhausted), both the Z and BZ flags are ON, the receiving string is filled with character 1 of the edit insertion table.

    EISaddr addr[3];
    
#define     ADDR1       addr[0]
    //word18  srcAddr; ///< address of sending string
    int     srcTally;///< number of chars in src (max 63)
    int     srcTA;   ///< type of Alphanumeric chars in src
    int     srcTN;   ///< type of Numeric chars in src
    int     srcSZ;   ///< size of chars in src (4-, 6-, or 9-bits)
    int     srcCN;   ///< starting at char pos CN

#define     ADDR2       addr[1]
    //word18  srcAddr2; ///< address of sending string (2nd)
    int     srcTA2;   ///< type of Alphanumeric chars in src (2nd)
    int     srcTN2;   ///< type of Numeric chars in src (2nd)
    int     srcSZ2;   ///< size of chars in src (4-, 6-, or 9-bits) (2nd)
    int     srcCN2;   ///< starting at char pos CN (2nd)

#define     ADDR3       addr[2]
    //word18  dstAddr; ///< address of receiving string
    int     dstTally;///< number of chars in dst (max 63)
    int     dstTA;   ///< type of Alphanumeric chars in dst
    int     dstTN;   ///< type of Numeric chars in dst
    int     dstSZ;   ///< size of chars in dst (4-, 6-, or 9-bits)
    int     dstCN;   ///< starting at char pos CN
    
    bool    mvne;    ///< for MSES micro-op. True when mvne, false when mve
    
    // I suppose we *could* tie function pointers to this, but then it'd be *too* much like cfront, eh?
};

/// Instruction decode structure. Used to represent instrucion information
struct DCDstruct
{
    //word36 IWB;         ///< instruction working buffer
    opCode *info;       ///< opCode *
    uint32  opcode;      ///< opcode
    bool   opcodeX;     ///< opcode extension
    word18 address;     ///< bits 0-17 of instruction XXX replace with rY
    bool   a;           ///< bit-29 - address via pointer register. Usually.
    bool   i;           ///< interrupt inhinit bit.
    word6  tag;         ///< instruction tag XXX replace with rTAG
    
    word18 stiTally;    ///< for sti instruction
    
    EISstruct e;       ///< info: if instruction is a MW EIS instruction
};

extern DCDstruct *currentInstruction;

// Emulator-only interrupt and fault info
typedef struct {
    bool xed; // executed xed for a fault handler
    bool any; // true if any of the below are true
    bool int_pending;
    int low_group; // Lowest group-number fault preset
    uint32 group7; // bitmask for multiple group 7 faults
    int fault[7]; // only one fault in groups 1..6 can be pending
    bool interrupts[32];
} events_t;
extern events_t events;

#define N_CPU_PORTS 4
// Physical Switches
typedef struct {
    // Switches on the Processor's maintenance and configuration panels
    uint FLT_BASE; // normally 7 MSB of 12bit fault base addr
    uint cpu_num;  // zero for CPU 'A', one for 'B' etc.
    word36 data_switches;
    //uint port_enable; // 4 bits; enable ports A-D
    //word36 port_config; // Read by rsw instruction; format unknown
    //uint port_interlace; // 4 bits  Read by rsw instruction; 
    uint assignment [N_CPU_PORTS];
    uint interlace [N_CPU_PORTS]; // 0/2/4
    uint enable [N_CPU_PORTS];
    uint init_enable [N_CPU_PORTS];
    uint store_size [N_CPU_PORTS]; // 0-7 encoding 32K-4M
    uint proc_mode; // 1 bit  Read by rsw instruction; format unknown
    uint proc_speed; // 4 bits Read by rsw instruction; format unknown
    uint invert_absolute; // If non-zero, invert the sense of the ABSOLUTE bit in the STI instruction
    uint b29_test; // If non-zero, enable untested code
    uint dis_enable; // If non-zero, DIS works
    uint auto_append_disable; // If non-zero, bit29 does not force APPEND_mode
    uint lprp_highonly; // If non-zero lprp only sets the high bits
    uint steady_clock; // If non-zero the clock is tied to the cycle counter
    uint degenerate_mode; // If non-zero use the experimental ABSOLUTE mode
    uint append_after; // 
    uint super_user; // 
    uint epp_hack; // 
    uint halt_on_unimp; // If non-zero, halt CPU on unimplemented instruction
                        // instead of faulting
    uint disable_wam; // If non-zero, disable PTWAM, STWAM
    uint bullet_time; // 
    uint disable_kbd_bkpt;
    uint report_faults; // If set, faults are reported and ignored
} switches_t;
extern switches_t switches;


// More emulator state variables for the cpu
// These probably belong elsewhere, perhaps control unit data or the
// cu-history regs...
typedef struct {
    cycles_t cycle;
    uint IC_abs; // translation of odd IC to an absolute address; see ADDRESS of cu history
    bool irodd_invalid; // cached odd instr invalid due to memory write by even instr
    uint read_addr; // last absolute read; might be same as CA for our purposes...; see APU RMA
    // bool instr_fetch; // true during an instruction fetch
    /* The following are all from the control unit history register: */
    bool trgo; // most recent instruction caused a transfer?
    bool ic_odd; // executing odd pair?
    bool poa; // prepare operand address
    uint opcode; // currently executing opcode
    struct {
        bool fhld; // An access violation or directed fault is waiting. AL39 mentions that the APU has this flag, but not where scpr stores it
    } apu_state;

    bool interrupt_flag; // an interrupt is pending in this cycle
    bool g7_flag; // a g7 fault is pending in this cycle;
     _fault faultNumber; // fault number saved by doFault
     _fault_subtype subFault; // saved by doFault
} cpu_state_t;
extern cpu_state_t cpu;

/* Control unit data (288 bits) */
typedef struct {
    /*
     NB: Some of the data normally stored here is represented
     elsewhere -- e.g.,the PPR is a variable outside of this
     struct.   Other data is live and only stored here.
     */
    /*      This is a collection of flags and registers from the
     appending unit and the control unit.  The scu and rcu
     instructions store and load these values to an 8 word
     memory block.
     The CU data may only be valid for use with the scu and
     rcu instructions.
     Comments indicate format as stored in 8 words by the scu
     instruction.
     */
    
    /* NOTE: PPR (procedure pointer register) is a combination of registers:
     From the Appending Unit
     PRR bits [0..2] of word 0
     PSR bits [3..17] of word 0
     P   bit 18 of word 0
     From the Control Unit
     IC  bits [0..17] of word 4
     */
    
#if 0
    
    /* First we list some registers we either don't use or that we have represented elsewhere */
    
    /* word 0 */
    // PPR portions copied from Appending Unit
    uint PPR_PRR;       /* Procedure ring register; 3 bits @ 0[0..2] */
    uint PPR_PSR;       /* Procedure segment register; 15 bits @ 0[3..17] */
    uint PPR_P;         /* Privileged bit; 1 bit @ 0[18] */
    // uint64 word0bits; /* Word 0, bits 18..32 (all for the APU) */
    uint FCT;           /* Fault counter; 3 bits at 0[33..35]; */
    
    /* word 1 */
    //uint64 word1bits; /* Word1, bits [0..19] and [35] */
    
    uint IA;        /* 4 bits @ 1[20..23] */
    uint IACHN;     /* 3 bits @ 1[24..26] */
    uint CNCHN;     /* 3 bits @ 1[27..29] */
    uint FIADDR     /* 5 bits @ 1[30..34] */
    
    /* word 2 */
    uint TPR_TRR;   // 3 bits @ 2[0..2];  temporary ring register
    uint TPR_TSR;   // 15 bits @ 2[3..17]; temporary segment register
    // unused: 10 bits at 2[18..27]
    // uint cpu_no; // 2 bits at 2[28..29]; from maint panel switches
    
    /* word 3 */
    
    /* word 4 */
    // IC belongs to CU
    int IC;         // 18 bits at 4[0..17]; instruction counter aka ilc
    // copy of IR bits 14 bits at 4[18..31]
    // unused: 4 bits at 4[32..36];
    
    /* word 5 */
    uint CA;        // 18 bits at 5[0..17]; computed address value (offset) used in the last address preparation cycle
    // cu bits for repeats, execute double, restarts, etc
#endif
    
    /* Above was documentation on all physical control unit data.
     * Below are the members we actually implement here.  Missing
     * members are either not (yet) emulated or are handled outside
     * of this control unit data struct.
     */
    
    /* word 0 */
                   // 0-2   PRR is stored in PPR
                   // 3-17  PSR is stored in PPR
                   // 18    P   is stored in PPR
                   // 19    XSF External segment flag -- not implemented
                   // 20    SDWAMM Match on SDWAM -- not implemented
    word1 SD_ON;   // 21    SDWAM enabled
                   // 22    PTWAMM Match on PTWAM -- not implemented
    word1 PT_ON;   // 23    PTWAM enabled
                   // 24    PI-AP Instruction fetch append cycle
                   // 25    DSPTW Fetch descriptor segment PTW
                   // 26    SDWNP Fetch SDW non paged
                   // 27    SDWP  Fetch SDW paged
                   // 28    PTW   Fetch PTW
                   // 29    PTW2  Fetch prepage PTW
                   // 30    FAP   Fetch final address - paged
                   // 31    FANP  Fetch final address - nonpaged
                   // 32    FABS  Fetch final address - absolute
                   // 33-35 FCT   Fault counter - counts retries

    /* word 1 */
    
    /* word 2 */
    word6 delta;     // 6 bits at 2[30..35]; addr increment for repeats
    
    /* word 4 */
    word18 IR;     /* Working instr register; addr & tag are modified */

    /* word 5 */
    bool repeat_first;        // "RF" flag -- first cycle of a repeat instruction; We also use with xed
    bool rpt;     // execute an rpt instruction
    bool rd;     // execute an rpd instruction
    uint CT_HOLD;   // 6 bits at 5[30..35]; contents of the "remember modifier" register

    //xde xdo
    // 0   0   no execute           -> 0 0
    // 1   0   execute XEC          -> 0 0
    // 1   1   execute even of XED  -> 0 1
    // 0   1   execute odd of XED   -> 0 0
    bool xde;     // execute even instr from xed pair
    bool xdo;     // execute even instr from xed pair
    
    
    /* word 6 */
    word36 IWB;

    /* word 7 */
    word36 IRODD; /* Instr holding register; odd word of last pair fetched */
    
} ctl_unit_data_t;
extern ctl_unit_data_t cu;


extern int stop_reason;     ///< sim_instr return value for JMP_STOP


void cancel_run(t_stat reason);
bool sample_interrupts (void);
t_stat simh_hooks (void);
int OPSIZE(DCDstruct *i);
t_stat ReadOP(DCDstruct *i, word18 addr, _processor_cycle_type cyctyp, bool b29);
t_stat WriteOP(DCDstruct *i, word18 addr, _processor_cycle_type acctyp, bool b29);
// RAW, core stuff ...
int core_read(word24 addr, word36 *data);
int core_write(word24 addr, word36 data);
int core_read2(word24 addr, word36 *even, d8 *odd);
int core_write2(word24 addr, word36 even, d8 odd);
int core_readN(word24 addr, word36 *data, int n);
int core_writeN(word24 addr, word36 *data, int n);
int core_read72(word24 addr, word72 *dst);

void freeDCDstruct(DCDstruct *p);
int is_priv_mode(void);
void set_went_appending (void);
addr_modes_t get_addr_mode(void);
void set_addr_mode(addr_modes_t mode);
int query_scu_unit_num (int cpu_unit_num, int cpu_port_num);
t_stat cable_to_cpu (int scu_unit_num, int scu_port_num, int iom_unit_num, int iom_port_num);
void init_opcodes (void);
void encode_instr(const instr_t *ip, word36 *wordp);
DCDstruct *decodeInstruction(word36 inst, DCDstruct *dst);     // decode instruction into structure
t_stat dpsCmd_Dump (int32 arg, char *buf);
t_stat dpsCmd_Init (int32 arg, char *buf);
t_stat dpsCmd_Segment (int32 arg, char *buf);
t_stat dpsCmd_Segments (int32 arg, char *buf);
_sdw0 *fetchSDW(word15 segno);
char *strSDW0(_sdw0 *SDW);
int query_scpage_map (word24 addr);
void cpu_init (void);

