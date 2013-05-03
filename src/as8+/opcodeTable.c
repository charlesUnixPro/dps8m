/**
 * \file as8_opcodetable.c
 * \project dps8
*/

//#include <string.h>

#include "as.h"

#include "y.tab.h"


PRIVATE
opCode allOpcodes[] =
{
    // non-EIS....
    // 000 - 017
	{},
	{"mme", OPCODE, 0, 0, false, 0001, 0001000},
	{"drl", OPCODE, 0, 0, false, 0002, 0002000},
	{},
	{"mme2", OPCODE, 0, 0, false, 0004, 0004000},
	{"mme3", OPCODE, 0, 0, false, 0005, 0005000},
	{},
	{"mme4", OPCODE, 0, 0, false, 0007, 0007000},
	{},
	{"nop", OPCODE, 0, 0, false, 0011, 0011000},
	{"puls1", OPCODE, 0, 0, false, 0012, 0012000},
	{"puls2", OPCODE, 0, 0, false, 0013, 0013000},
	{},
	{"cioc", OPCODE, 0, 0, false, 0015, 0015000},
	{},
	{},
    // 020 - 037
	{"adlx0", OPCODE, 0, 0, false, 0020, 0020000},
   	{"adlxap", OPCODE, 0, 0, false, 0020, 0020000},
    
	{"adlx1", OPCODE, 0, 0, false, 0021, 0021000},
	{"adlxab", OPCODE, 0, 0, false, 0021, 0021000},

	{"adlx2", OPCODE, 0, 0, false, 0022, 0022000},
    {"adlxbp", OPCODE, 0, 0, false, 0022, 0022000},
    
	{"adlx3", OPCODE, 0, 0, false, 0023, 0023000},
	{"adlxbb", OPCODE, 0, 0, false, 0023, 0023000},

	{"adlx4", OPCODE, 0, 0, false, 0024, 0024000},
	{"adlxlp", OPCODE, 0, 0, false, 0024, 0024000},

	{"adlx5", OPCODE, 0, 0, false, 0025, 0025000},
	{"adlxlb", OPCODE, 0, 0, false, 0025, 0025000},

	{"adlx6", OPCODE, 0, 0, false, 0026, 0026000},
	{"adlxsp", OPCODE, 0, 0, false, 0026, 0026000},

	{"adlx7", OPCODE, 0, 0, false, 0027, 0027000},
	{"adlxsb", OPCODE, 0, 0, false, 0027, 0027000},

	{},
	{},
	{"ldqc", OPCODE, 0, 0, false, 0032, 0032000},
	{"adl", OPCODE, 0, 0, false, 0033, 0033000},
	{"ldac", OPCODE, 0, 0, false, 0034, 0034000},
	{"adla", OPCODE, 0, 0, false, 0035, 0035000},
	{"adlq", OPCODE, 0, 0, false, 0036, 0036000},
	{"adlaq", OPCODE, 0, 0, false, 0037, 0037000},
    // 040 - 057
	{"asx0", OPCODE, 0, 0, false, 0040, 0040000},
  	{"asxap", OPCODE, 0, 0, false, 0040, 0040000},
    
	{"asx1", OPCODE, 0, 0, false, 0041, 0041000},
	{"asxab", OPCODE, 0, 0, false, 0041, 0041000},

	{"asx2", OPCODE, 0, 0, false, 0042, 0042000},
	{"asxbp", OPCODE, 0, 0, false, 0042, 0042000},

	{"asx3", OPCODE, 0, 0, false, 0043, 0043000},
	{"asxbb", OPCODE, 0, 0, false, 0043, 0043000},

	{"asx4", OPCODE, 0, 0, false, 0044, 0044000},
	{"asxlp", OPCODE, 0, 0, false, 0044, 0044000},

	{"asx5", OPCODE, 0, 0, false, 0045, 0045000},
	{"asxlb", OPCODE, 0, 0, false, 0045, 0045000},

	{"asx6", OPCODE, 0, 0, false, 0046, 0046000},
	{"asxsp", OPCODE, 0, 0, false, 0046, 0046000},

	{"asx7", OPCODE, 0, 0, false, 0047, 0047000},
	{"asxsb", OPCODE, 0, 0, false, 0047, 0047000},
    
	{"adwp0", OPCODE, 0, 0, false, 0050, 0050000},
    {"adwpap", OPCODE, 0, 0, false, 0050, 0050000},
    
	{"adwp1", OPCODE, 0, 0, false, 0051, 0051000},
    {"adwpab", OPCODE, 0, 0, false, 0051, 0051000},
    
	{"adwp2", OPCODE, 0, 0, false, 0052, 0052000},
	{"adwpbp", OPCODE, 0, 0, false, 0052, 0052000},

	{"adwp3", OPCODE, 0, 0, false, 0053, 0053000},
	{"adwpbb", OPCODE, 0, 0, false, 0053, 0053000},

	{"aos", OPCODE, 0, 0, false, 0054, 0054000},
	{"asa", OPCODE, 0, 0, false, 0055, 0055000},
	{"asq", OPCODE, 0, 0, false, 0056, 0056000},
	{"sscr", OPCODE, 0, 0, false, 0057, 0057000},
    // 060 - 077
	{"adx0", OPCODE, 0, 0, false, 0060, 0060000},
    {"adxap", OPCODE, 0, 0, false, 0060, 0060000},
    
	{"adx1", OPCODE, 0, 0, false, 0061, 0061000},
	{"adxab", OPCODE, 0, 0, false, 0061, 0061000},
    
	{"adx2", OPCODE, 0, 0, false, 0062, 0062000},
	{"adxbp", OPCODE, 0, 0, false, 0062, 0062000},

	{"adx3", OPCODE, 0, 0, false, 0063, 0063000},
	{"adxbb", OPCODE, 0, 0, false, 0063, 0063000},

	{"adx4", OPCODE, 0, 0, false, 0064, 0064000},
	{"adxlp", OPCODE, 0, 0, false, 0064, 0064000},

	{"adx5", OPCODE, 0, 0, false, 0065, 0065000},
	{"adxlb", OPCODE, 0, 0, false, 0065, 0065000},

	{"adx6", OPCODE, 0, 0, false, 0066, 0066000},
	{"adxsp", OPCODE, 0, 0, false, 0066, 0066000},

	{"adx7", OPCODE, 0, 0, false, 0067, 0067000},
	{"adxsb", OPCODE, 0, 0, false, 0067, 0067000},
    
	{},
	{"awca", OPCODE, 0, 0, false, 0071, 0071000},
	{"awcq", OPCODE, 0, 0, false, 0072, 0072000},
	{"lreg", OPCODE, 0, 0, false, 0073, 0073000},
	{},
	{"ada", OPCODE, 0, 0, false, 0075, 0075000},
	{"adq", OPCODE, 0, 0, false, 0076, 0076000},
	{"adaq", OPCODE, 0, 0, false, 0077, 0077000},
    // 100 - 117
	{"cmpx0", OPCODE, 0, 0, false, 0100, 0100000},
	{"cmpxap", OPCODE, 0, 0, false, 0100, 0100000},
    
	{"cmpx1", OPCODE, 0, 0, false, 0101, 0101000},
	{"cmpxab", OPCODE, 0, 0, false, 0101, 0101000},
	
    {"cmpx2", OPCODE, 0, 0, false, 0102, 0102000},
    {"cmpxbp", OPCODE, 0, 0, false, 0102, 0102000},

	{"cmpx3", OPCODE, 0, 0, false, 0103, 0103000},
	{"cmpxbb", OPCODE, 0, 0, false, 0103, 0103000},

	{"cmpx4", OPCODE, 0, 0, false, 0104, 0104000},
	{"cmpxlp", OPCODE, 0, 0, false, 0104, 0104000},

	{"cmpx5", OPCODE, 0, 0, false, 0105, 0105000},
	{"cmpxlb", OPCODE, 0, 0, false, 0105, 0105000},

	{"cmpx6", OPCODE, 0, 0, false, 0106, 0106000},
	{"cmpxsp", OPCODE, 0, 0, false, 0106, 0106000},

	{"cmpx7", OPCODE, 0, 0, false, 0107, 0107000},
	{"cmpxsb", OPCODE, 0, 0, false, 0107, 0107000},

	{},
	{"cwl", OPCODE, 0, 0, false, 0111, 0111000},
	{},
	{},
	{},
	{"cmpa", OPCODE, 0, 0, false, 0115, 0115000},
	{"cmpq", OPCODE, 0, 0, false, 0116, 0116000},
	{"cmpaq", OPCODE, 0, 0, false, 0117, 0117000},
    // 120 - 137
	{"sblx0", OPCODE, 0, 0, false, 0120, 0120000},
	{"sblxap", OPCODE, 0, 0, false, 0120, 0120000},

	{"sblx1", OPCODE, 0, 0, false, 0121, 0121000},
	{"sblxab", OPCODE, 0, 0, false, 0121, 0121000},

	{"sblx2", OPCODE, 0, 0, false, 0122, 0122000},
	{"sblxbp", OPCODE, 0, 0, false, 0122, 0122000},

	{"sblx3", OPCODE, 0, 0, false, 0123, 0123000},
	{"sblxbb", OPCODE, 0, 0, false, 0123, 0123000},

	{"sblx4", OPCODE, 0, 0, false, 0124, 0124000},
	{"sblxlp", OPCODE, 0, 0, false, 0124, 0124000},

	{"sblx5", OPCODE, 0, 0, false, 0125, 0125000},
	{"sblxlb", OPCODE, 0, 0, false, 0125, 0125000},

	{"sblx6", OPCODE, 0, 0, false, 0126, 0126000},
	{"sblxsp", OPCODE, 0, 0, false, 0126, 0126000},

	{"sblx7", OPCODE, 0, 0, false, 0127, 0127000},
	{"sblxsb", OPCODE, 0, 0, false, 0127, 0127000},

	{},
	{},
	{},
	{},
	{},
	{"sbla", OPCODE, 0, 0, false, 0135, 0135000},
	{"sblq", OPCODE, 0, 0, false, 0136, 0136000},
	{"sblaq", OPCODE, 0, 0, false, 0137, 0137000},
    // 140 - 157
	{"ssx0", OPCODE, 0, 0, false, 0140, 0140000},
	{"ssxap", OPCODE, 0, 0, false, 0140, 0140000},
    
	{"ssx1", OPCODE, 0, 0, false, 0141, 0141000},
	{"ssxab", OPCODE, 0, 0, false, 0141, 0141000},

	{"ssx2", OPCODE, 0, 0, false, 0142, 0142000},
	{"ssxbp", OPCODE, 0, 0, false, 0142, 0142000},

	{"ssx3", OPCODE, 0, 0, false, 0143, 0143000},
	{"ssxbb", OPCODE, 0, 0, false, 0143, 0143000},

	{"ssx4", OPCODE, 0, 0, false, 0144, 0144000},
	{"ssxlp", OPCODE, 0, 0, false, 0144, 0144000},

	{"ssx5", OPCODE, 0, 0, false, 0145, 0145000},
	{"ssxlb", OPCODE, 0, 0, false, 0145, 0145000},

	{"ssx6", OPCODE, 0, 0, false, 0146, 0146000},
	{"ssxsp", OPCODE, 0, 0, false, 0146, 0146000},

	{"ssx7", OPCODE, 0, 0, false, 0147, 0147000},
	{"ssxsb", OPCODE, 0, 0, false, 0147, 0147000},
    
	{"adwp4", OPCODE, 0, 0, false, 0150, 0150000},
	{"adwplp", OPCODE, 0, 0, false, 0150, 0150000},

	{"adwp5", OPCODE, 0, 0, false, 0151, 0151000},
	{"adwplb", OPCODE, 0, 0, false, 0151, 0151000},

	{"adwp6", OPCODE, 0, 0, false, 0152, 0152000},
	{"adwpsp", OPCODE, 0, 0, false, 0152, 0152000},

	{"adwp7", OPCODE, 0, 0, false, 0153, 0153000},
	{"adwpsb", OPCODE, 0, 0, false, 0153, 0153000},

	{"sdbr", OPCODE, 0, 0, false, 0154, 0154000},
	{"ssa", OPCODE, 0, 0, false, 0155, 0155000},
	{"ssq", OPCODE, 0, 0, false, 0156, 0156000},
	{},
    // 160 - 177
	{"sbx0", OPCODE, 0, 0, false, 0160, 0160000},
	{"sbxap", OPCODE, 0, 0, false, 0160, 0160000},
    
	{"sbx1", OPCODE, 0, 0, false, 0161, 0161000},
	{"sbxab", OPCODE, 0, 0, false, 0161, 0161000},

	{"sbx2", OPCODE, 0, 0, false, 0162, 0162000},
	{"sbxbp", OPCODE, 0, 0, false, 0162, 0162000},

	{"sbx3", OPCODE, 0, 0, false, 0163, 0163000},
	{"sbxbb", OPCODE, 0, 0, false, 0163, 0163000},

	{"sbx4", OPCODE, 0, 0, false, 0164, 0164000},
	{"sbxlp", OPCODE, 0, 0, false, 0164, 0164000},

	{"sbx5", OPCODE, 0, 0, false, 0165, 0165000},
	{"sbxlb", OPCODE, 0, 0, false, 0165, 0165000},

	{"sbx6", OPCODE, 0, 0, false, 0166, 0166000},
	{"sbxsp", OPCODE, 0, 0, false, 0166, 0166000},

	{"sbx7", OPCODE, 0, 0, false, 0167, 0167000},
	{"sbxsb", OPCODE, 0, 0, false, 0167, 0167000},
    
	{},
	{"swca", OPCODE, 0, 0, false, 0171, 0171000},
	{"swcq", OPCODE, 0, 0, false, 0172, 0172000},
	{"lpri", OPCODE, 0, 0, false, 0173, 0173000},
	{},
	{"sba", OPCODE, 0, 0, false, 0175, 0175000},
	{"sbq", OPCODE, 0, 0, false, 0176, 0176000},
	{"sbaq", OPCODE, 0, 0, false, 0177, 0177000},
    // 200 - 217
	{"cnax0", OPCODE, 0, 0, false, 0200, 0200000},
	{"cnaxap", OPCODE, 0, 0, false, 0200, 0200000},

	{"cnax1", OPCODE, 0, 0, false, 0201, 0201000},
	{"cnaxab", OPCODE, 0, 0, false, 0201, 0201000},

	{"cnax2", OPCODE, 0, 0, false, 0202, 0202000},
	{"cnaxbp", OPCODE, 0, 0, false, 0202, 0202000},

	{"cnax3", OPCODE, 0, 0, false, 0203, 0203000},
	{"cnaxbb", OPCODE, 0, 0, false, 0203, 0203000},

	{"cnax4", OPCODE, 0, 0, false, 0204, 0204000},
	{"cnaxlp", OPCODE, 0, 0, false, 0204, 0204000},

	{"cnax5", OPCODE, 0, 0, false, 0205, 0205000},
	{"cnaxlb", OPCODE, 0, 0, false, 0205, 0205000},

	{"cnax6", OPCODE, 0, 0, false, 0206, 0206000},
	{"cnaxsp", OPCODE, 0, 0, false, 0206, 0206000},

	{"cnax7", OPCODE, 0, 0, false, 0207, 0207000},
	{"cnaxsb", OPCODE, 0, 0, false, 0207, 0207000},
    
	{},
	{"cmk", OPCODE, 0, 0, false, 0211, 0211000},
	{"absa", OPCODE, 0, 0, false, 0212, 0212000},
	{"epaq", OPCODE, 0, 0, false, 0213, 0213000},
	{"sznc", OPCODE, 0, 0, false, 0214, 0214000},
	{"cnaa", OPCODE, 0, 0, false, 0215, 0215000},
	{"cnaq", OPCODE, 0, 0, false, 0216, 0216000},
	{"cnaaq", OPCODE, 0, 0, false, 0217, 0217000},
    // 220 - 237
	{"ldx0", OPCODE, 0, 0, false, 0220, 0220000},
	{"ldxap", OPCODE, 0, 0, false, 0220, 0220000},

	{"ldx1", OPCODE, 0, 0, false, 0221, 0221000},
	{"ldxab", OPCODE, 0, 0, false, 0221, 0221000},

	{"ldx2", OPCODE, 0, 0, false, 0222, 0222000},
	{"ldxbp", OPCODE, 0, 0, false, 0222, 0222000},

	{"ldx3", OPCODE, 0, 0, false, 0223, 0223000},
	{"ldxbb", OPCODE, 0, 0, false, 0223, 0223000},

	{"ldx4", OPCODE, 0, 0, false, 0224, 0224000},
	{"ldxlp", OPCODE, 0, 0, false, 0224, 0224000},

	{"ldx5", OPCODE, 0, 0, false, 0225, 0225000},
	{"ldxlb", OPCODE, 0, 0, false, 0225, 0225000},

	{"ldx6", OPCODE, 0, 0, false, 0226, 0226000},
	{"ldxsp", OPCODE, 0, 0, false, 0226, 0226000},

	{"ldx7", OPCODE, 0, 0, false, 0227, 0227000},
	{"ldxsb", OPCODE, 0, 0, false, 0227, 0227000},
    
	{"lbar", OPCODE, 0, 0, false, 0230, 0230000},
	{"rsw", OPCODE, 0, 0, false, 0231, 0231000},
	{"ldbr", OPCODE, 0, 0, false, 0232, 0232000},
	{"rmcm", OPCODE, 0, 0, false, 0233, 0233000},
	{"szn", OPCODE, 0, 0, false, 0234, 0234000},
	{"lda", OPCODE, 0, 0, false, 0235, 0235000},
	{"ldq", OPCODE, 0, 0, false, 0236, 0236000},
	{"ldaq", OPCODE, 0, 0, false, 0237, 0237000},
    // 240 - 257
	{"orsx0", OPCODE, 0, 0, false, 0240, 0240000},
	{"orsxap", OPCODE, 0, 0, false, 0240, 0240000},

	{"orsx1", OPCODE, 0, 0, false, 0241, 0241000},
	{"orsxab", OPCODE, 0, 0, false, 0241, 0241000},

	{"orsx2", OPCODE, 0, 0, false, 0242, 0242000},
	{"orsxbp", OPCODE, 0, 0, false, 0242, 0242000},

	{"orsx3", OPCODE, 0, 0, false, 0243, 0243000},
	{"orsxbb", OPCODE, 0, 0, false, 0243, 0243000},

	{"orsx4", OPCODE, 0, 0, false, 0244, 0244000},
	{"orsxlp", OPCODE, 0, 0, false, 0244, 0244000},

	{"orsx5", OPCODE, 0, 0, false, 0245, 0245000},
	{"orsxlb", OPCODE, 0, 0, false, 0245, 0245000},

	{"orsx6", OPCODE, 0, 0, false, 0246, 0246000},
	{"orsxsp", OPCODE, 0, 0, false, 0246, 0246000},

	{"orsx7", OPCODE, 0, 0, false, 0247, 0247000},
	{"orsxsb", OPCODE, 0, 0, false, 0247, 0247000},
    
	{"spri0", OPCODE, 0, 0, false, 0250, 0250000},
	{"spriap", OPCODE, 0, 0, false, 0250, 0250000},
	
    {"spbp1", OPCODE, 0, 0, false, 0251, 0251000},
    {"spbpab", OPCODE, 0, 0, false, 0251, 0251000},

	{"spri2", OPCODE, 0, 0, false, 0252, 0252000},
    {"spribp", OPCODE, 0, 0, false, 0252, 0252000},
    
	{"spbp3", OPCODE, 0, 0, false, 0253, 0253000},
	{"spbpbb", OPCODE, 0, 0, false, 0253, 0253000},

	{"spri", OPCODE, 0, 0, false, 0254, 0254000},
	{"orsa", OPCODE, 0, 0, false, 0255, 0255000},
	{"orsq", OPCODE, 0, 0, false, 0256, 0256000},
	{"lsdp", OPCODE, 0, 0, false, 0257, 0257000},
    // 260 - 277
	{"orx0", OPCODE, 0, 0, false, 0260, 0260000},
	{"orxap", OPCODE, 0, 0, false, 0260, 0260000},

	{"orx1", OPCODE, 0, 0, false, 0261, 0261000},
	{"orxab", OPCODE, 0, 0, false, 0261, 0261000},

	{"orx2", OPCODE, 0, 0, false, 0262, 0262000},
	{"orxbp", OPCODE, 0, 0, false, 0262, 0262000},

	{"orx3", OPCODE, 0, 0, false, 0263, 0263000},
	{"orxbb", OPCODE, 0, 0, false, 0263, 0263000},

	{"orx4", OPCODE, 0, 0, false, 0264, 0264000},
	{"orxlp", OPCODE, 0, 0, false, 0264, 0264000},

	{"orx5", OPCODE, 0, 0, false, 0265, 0265000},
	{"orxlb", OPCODE, 0, 0, false, 0265, 0265000},

	{"orx6", OPCODE, 0, 0, false, 0266, 0266000},
	{"orxsp", OPCODE, 0, 0, false, 0266, 0266000},

	{"orx7", OPCODE, 0, 0, false, 0267, 0267000},
	{"orxsb", OPCODE, 0, 0, false, 0267, 0267000},
    
	{"tsp0", OPCODE, 0, 0, false, 0270, 0270000},
	{"tspap", OPCODE, 0, 0, false, 0270, 0270000},

	{"tsp1", OPCODE, 0, 0, false, 0271, 0271000},
	{"tspab", OPCODE, 0, 0, false, 0271, 0271000},

	{"tsp2", OPCODE, 0, 0, false, 0272, 0272000},
    {"tspbp", OPCODE, 0, 0, false, 0272, 0272000},
    
	{"tsp3", OPCODE, 0, 0, false, 0273, 0273000},
	{"tspbb", OPCODE, 0, 0, false, 0273, 0273000},

	{},
	{"ora", OPCODE, 0, 0, false, 0275, 0275000},
	{"orq", OPCODE, 0, 0, false, 0276, 0276000},
	{"oraq", OPCODE, 0, 0, false, 0277, 0277000},
    // 300 - 317
	{"canx0", OPCODE, 0, 0, false, 0300, 0300000},
	{"canxap", OPCODE, 0, 0, false, 0300, 0300000},

	{"canx1", OPCODE, 0, 0, false, 0301, 0301000},
	{"canxab", OPCODE, 0, 0, false, 0301, 0301000},

	{"canx2", OPCODE, 0, 0, false, 0302, 0302000},
	{"canxbp", OPCODE, 0, 0, false, 0302, 0302000},

	{"canx3", OPCODE, 0, 0, false, 0303, 0303000},
	{"canxbb", OPCODE, 0, 0, false, 0303, 0303000},

	{"canx4", OPCODE, 0, 0, false, 0304, 0304000},
	{"canxlp", OPCODE, 0, 0, false, 0304, 0304000},

	{"canx5", OPCODE, 0, 0, false, 0305, 0305000},
	{"canxlb", OPCODE, 0, 0, false, 0305, 0305000},

	{"canx6", OPCODE, 0, 0, false, 0306, 0306000},
	{"canxsp", OPCODE, 0, 0, false, 0306, 0306000},

	{"canx7", OPCODE, 0, 0, false, 0307, 0307000},
	{"canxsb", OPCODE, 0, 0, false, 0307, 0307000},
    
	{"eawp0", OPCODE, 0, 0, false, 0310, 0310000},
    {"eawpap", OPCODE, 0, 0, false, 0310, 0310000},
    
	{"easp0", OPCODE, 0, 0, false, 0311, 0311000},
    {"easpap", OPCODE, 0, 0, false, 0311, 0311000},
    
	{"eawp2", OPCODE, 0, 0, false, 0312, 0312000},
	{"eawpbp", OPCODE, 0, 0, false, 0312, 0312000},

	{"easp2", OPCODE, 0, 0, false, 0313, 0313000},
	{"easpbp", OPCODE, 0, 0, false, 0313, 0313000},

	{},
	{"cana", OPCODE, 0, 0, false, 0315, 0315000},
	{"canq", OPCODE, 0, 0, false, 0316, 0316000},
	{"canaq", OPCODE, 0, 0, false, 0317, 0317000},
    // 320 - 337
	{"lcx0", OPCODE, 0, 0, false, 0320, 0320000},
	{"lcxap", OPCODE, 0, 0, false, 0320, 0320000},

	{"lcx1", OPCODE, 0, 0, false, 0321, 0321000},
	{"lcxab", OPCODE, 0, 0, false, 0321, 0321000},

	{"lcx2", OPCODE, 0, 0, false, 0322, 0322000},
	{"lcxbp", OPCODE, 0, 0, false, 0322, 0322000},

	{"lcx3", OPCODE, 0, 0, false, 0323, 0323000},
	{"lcxbb", OPCODE, 0, 0, false, 0323, 0323000},

	{"lcx4", OPCODE, 0, 0, false, 0324, 0324000},
	{"lcxlp", OPCODE, 0, 0, false, 0324, 0324000},

	{"lcx5", OPCODE, 0, 0, false, 0325, 0325000},
	{"lcxlb", OPCODE, 0, 0, false, 0325, 0325000},

	{"lcx6", OPCODE, 0, 0, false, 0326, 0326000},
	{"lcxsp", OPCODE, 0, 0, false, 0326, 0326000},

	{"lcx7", OPCODE, 0, 0, false, 0327, 0327000},
	{"lcxsb", OPCODE, 0, 0, false, 0327, 0327000},
    
	{"eawp4", OPCODE, 0, 0, false, 0330, 0330000},
	{"eawplp", OPCODE, 0, 0, false, 0330, 0330000},

	{"easp4", OPCODE, 0, 0, false, 0331, 0331000},
	{"easplp", OPCODE, 0, 0, false, 0331, 0331000},

	{"eawp6", OPCODE, 0, 0, false, 0332, 0332000},
	{"eawpsp", OPCODE, 0, 0, false, 0332, 0332000},

	{"easp6", OPCODE, 0, 0, false, 0333, 0333000},
	{"easpsp", OPCODE, 0, 0, false, 0333, 0333000},

	{},
	{"lca", OPCODE, 0, 0, false, 0335, 0335000},
	{"lcq", OPCODE, 0, 0, false, 0336, 0336000},
	{"lcaq", OPCODE, 0, 0, false, 0337, 0337000},
    // 340 - 357
	{"ansx0", OPCODE, 0, 0, false, 0340, 0340000},
	{"ansxap", OPCODE, 0, 0, false, 0340, 0340000},

	{"ansx1", OPCODE, 0, 0, false, 0341, 0341000},
	{"ansxab", OPCODE, 0, 0, false, 0341, 0341000},

	{"ansx2", OPCODE, 0, 0, false, 0342, 0342000},
	{"ansxbp", OPCODE, 0, 0, false, 0342, 0342000},
	
    {"ansx3", OPCODE, 0, 0, false, 0343, 0343000},
    {"ansxbb", OPCODE, 0, 0, false, 0343, 0343000},

    {"ansx4", OPCODE, 0, 0, false, 0344, 0344000},
    {"ansxlp", OPCODE, 0, 0, false, 0344, 0344000},

	{"ansx5", OPCODE, 0, 0, false, 0345, 0345000},
	{"ansxlb", OPCODE, 0, 0, false, 0345, 0345000},

	{"ansx6", OPCODE, 0, 0, false, 0346, 0346000},
	{"ansxsp", OPCODE, 0, 0, false, 0346, 0346000},

	{"ansx7", OPCODE, 0, 0, false, 0347, 0347000},
	{"ansxsb", OPCODE, 0, 0, false, 0347, 0347000},
    
	{"epp0", OPCODE, 0, 0, false, 0350, 0350000},
  	{"eppap", OPCODE, 0, 0, false, 0350, 0350000},
    
	{"epbp1", OPCODE, 0, 0, false, 0351, 0351000},
	{"epbpab", OPCODE, 0, 0, false, 0351, 0351000},

	{"epp2", OPCODE, 0, 0, false, 0352, 0352000},
    {"eppbp", OPCODE, 0, 0, false, 0352, 0352000},
    
	{"epbp3", OPCODE, 0, 0, false, 0353, 0353000},
	{"epbpbb", OPCODE, 0, 0, false, 0353, 0353000},
	
    {"stac", OPCODE, 0, 0, false, 0354, 0354000},
	{"ansa", OPCODE, 0, 0, false, 0355, 0355000},
	{"ansq", OPCODE, 0, 0, false, 0356, 0356000},
	{"stcd", OPCODE, 0, 0, false, 0357, 0357000},
    // 360 - 377
	{"anx0", OPCODE, 0, 0, false, 0360, 0360000},
	{"anxap", OPCODE, 0, 0, false, 0360, 0360000},
	
    {"anx1", OPCODE, 0, 0, false, 0361, 0361000},
    {"anxab", OPCODE, 0, 0, false, 0361, 0361000},

	{"anx2", OPCODE, 0, 0, false, 0362, 0362000},
	{"anxbp", OPCODE, 0, 0, false, 0362, 0362000},

	{"anx3", OPCODE, 0, 0, false, 0363, 0363000},
	{"anxbb", OPCODE, 0, 0, false, 0363, 0363000},

	{"anx4", OPCODE, 0, 0, false, 0364, 0364000},
	{"anxlp", OPCODE, 0, 0, false, 0364, 0364000},

	{"anx5", OPCODE, 0, 0, false, 0365, 0365000},
	{"anxlb", OPCODE, 0, 0, false, 0365, 0365000},

	{"anx6", OPCODE, 0, 0, false, 0366, 0366000},
	{"anxsp", OPCODE, 0, 0, false, 0366, 0366000},

	{"anx7", OPCODE, 0, 0, false, 0367, 0367000},
	{"anxsb", OPCODE, 0, 0, false, 0367, 0367000},

	{"epp4", OPCODE, 0, 0, false, 0370, 0370000},
	{"epplp", OPCODE, 0, 0, false, 0370, 0370000},
	{"epbp5", OPCODE, 0, 0, false, 0371, 0371000},
	{"epbplb", OPCODE, 0, 0, false, 0371, 0371000},

	{"epp6", OPCODE, 0, 0, false, 0372, 0372000},
	{"eppsp", OPCODE, 0, 0, false, 0372, 0372000},

	{"epbp7", OPCODE, 0, 0, false, 0373, 0373000},
	{"epbpsb", OPCODE, 0, 0, false, 0373, 0373000},

	{},
	{"ana", OPCODE, 0, 0, false, 0375, 0375000},
	{"anq", OPCODE, 0, 0, false, 0376, 0376000},
	{"anaq", OPCODE, 0, 0, false, 0377, 0377000},
    // 400 - 417
	{},
	{"mpf", OPCODE, 0, 0, false, 0401, 0401000},
	{"mpy", OPCODE, 0, 0, false, 0402, 0402000},
	{},
	{},
	{"cmg", OPCODE, 0, 0, false, 0405, 0405000},
	{},
	{},
	{},
	{"lde", OPCODE, 0, 0, false, 0411, 0411000},
	{},
	{"rscr", OPCODE, 0, 0, false, 0413, 0413000},
	{},
	{"ade", OPCODE, 0, 0, false, 0415, 0415000},
	{},
	{},
    // 420 - 437
	{},
	{"ufm", OPCODE, 0, 0, false, 0421, 0421000},
	{},
	{"dufm", OPCODE, 0, 0, false, 0423, 0423000},
	{},
	{"fcmg", OPCODE, 0, 0, false, 0425, 0425000},
	{},
	{"dfcmg", OPCODE, 0, 0, false, 0427, 0427000},
	{"fszn", OPCODE, 0, 0, false, 0430, 0430000},
	{"fld", OPCODE, 0, 0, false, 0431, 0431000},
	{},
	{"dfld", OPCODE, 0, 0, false, 0433, 0433000},
	{},
	{"ufa", OPCODE, 0, 0, false, 0435, 0435000},
	{},
	{"dufa", OPCODE, 0, 0, false, 0437, 0437000},
    // 440 - 457
	{"sxl0", OPCODE, 0, 0, false, 0440, 0440000},
  	{"sxlap", OPCODE, 0, 0, false, 0440, 0440000},
    
	{"sxl1", OPCODE, 0, 0, false, 0441, 0441000},
	{"sxlab", OPCODE, 0, 0, false, 0441, 0441000},

	{"sxl2", OPCODE, 0, 0, false, 0442, 0442000},
	{"sxlbp", OPCODE, 0, 0, false, 0442, 0442000},

	{"sxl3", OPCODE, 0, 0, false, 0443, 0443000},
	{"sxlbb", OPCODE, 0, 0, false, 0443, 0443000},

	{"sxl4", OPCODE, 0, 0, false, 0444, 0444000},
	{"sxllp", OPCODE, 0, 0, false, 0444, 0444000},

	{"sxl5", OPCODE, 0, 0, false, 0445, 0445000},
	{"sxllb", OPCODE, 0, 0, false, 0445, 0445000},

	{"sxl6", OPCODE, 0, 0, false, 0446, 0446000},
	{"sxlsp", OPCODE, 0, 0, false, 0446, 0446000},

	{"sxl7", OPCODE, 0, 0, false, 0447, 0447000},
	{"sxlsb", OPCODE, 0, 0, false, 0447, 0447000},

	{"stz", OPCODE, 0, 0, false, 0450, 0450000},
	{"smic", OPCODE, 0, 0, false, 0451, 0451000},
	{"scpr", OPCODE, 0, 0, false, 0452, 0452000},
	{},
	{"stt", OPCODE, 0, 0, false, 0454, 0454000},
	{"fst", OPCODE, 0, 0, false, 0455, 0455000},
	{"ste", OPCODE, 0, 0, false, 0456, 0456000},
	{"dfst", OPCODE, 0, 0, false, 0457, 0457000},
    // 460 - 477
	{},
	{"fmp", OPCODE, 0, 0, false, 0461, 0461000},
	{},
	{"dfmp", OPCODE, 0, 0, false, 0463, 0463000},
	{},
	{},
	{},
	{},
	{"fstr", OPCODE, 0, 0, false, 0470, 0470000},
	{"frd", OPCODE, 0, 0, false, 0471, 0471000},
	{"dfstr", OPCODE, 0, 0, false, 0472, 0472000},
	{"dfrd", OPCODE, 0, 0, false, 0473, 0473000},
	{},
	{"fad", OPCODE, 0, 0, false, 0475, 0475000},
	{},
	{"dfad", OPCODE, 0, 0, false, 0477, 0477000},
    // 500 - 517
	{"rpl",  OPCODERPT, 0, 0, false, 0500, 0500200},    // C == 1
  	{"rplx", OPCODERPT, 0, 0, false, 0500, 0500200},    // C == 0
	{},
	{},
	{},
	{},
	{"bcd", OPCODE, 0, 0, false, 0505, 0505000},
	{"div", OPCODE, 0, 0, false, 0506, 0506000},
	{"dvf", OPCODE, 0, 0, false, 0507, 0507000},
	{},
	{},
	{},
	{"fneg", OPCODE, 0, 0, false, 0513, 0513000},
	{},
	{"fcmp", OPCODE, 0, 0, false, 0515, 0515000},
	{},
	{"dfcmp", OPCODE, 0, 0, false, 0517, 0517000},
    // 520 - 537
	{"rpt",  OPCODERPT, 0, 0, false, 0520, 0520200},    // C == 1
    {"rptx", OPCODERPT, 0, 0, false, 0520, 0520200},    // C == 0
	{},
	{},
	{},
	{},
	{"fdi", OPCODE, 0, 0, false, 0525, 0525000},
	{},
	{"dfdi", OPCODE, 0, 0, false, 0527, 0527000},
	{},
	{"neg", OPCODE, 0, 0, false, 0531, 0531000},
	{"cams", OPCODE, 0, 0, false, 0532, 0532000},
	{"negl", OPCODE, 0, 0, false, 0533, 0533000},
	{},
	{"ufs", OPCODE, 0, 0, false, 0535, 0535000},
	{},
	{"dufs", OPCODE, 0, 0, false, 0537, 0537000},
    // 540 - 557
	{"sprp0", OPCODE, 0, 0, false, 0540, 0540000},
	{"sprpap", OPCODE, 0, 0, false, 0540, 0540000},

	{"sprp1", OPCODE, 0, 0, false, 0541, 0541000},
	{"sprpab", OPCODE, 0, 0, false, 0541, 0541000},

	{"sprp2", OPCODE, 0, 0, false, 0542, 0542000},
    {"sprpbp", OPCODE, 0, 0, false, 0542, 0542000},
    
	{"sprp3", OPCODE, 0, 0, false, 0543, 0543000},
	{"sprpbb", OPCODE, 0, 0, false, 0543, 0543000},

	{"sprp4", OPCODE, 0, 0, false, 0544, 0544000},
	{"sprplp", OPCODE, 0, 0, false, 0544, 0544000},

	{"sprp5", OPCODE, 0, 0, false, 0545, 0545000},
	{"sprplb", OPCODE, 0, 0, false, 0545, 0545000},

	{"sprp6", OPCODE, 0, 0, false, 0546, 0546000},
	{"sprpsp", OPCODE, 0, 0, false, 0546, 0546000},

	{"sprp7", OPCODE, 0, 0, false, 0547, 0547000},
	{"sprpsb", OPCODE, 0, 0, false, 0547, 0547000},
    
	{"sbar", OPCODE, 0, 0, false, 0550, 0550000},
	{"stba", OPCODESTC, 0, 0, false, 0551, 0551000},
	{"stbq", OPCODESTC, 0, 0, false, 0552, 0552000},
	{"smcm", OPCODE, 0, 0, false, 0553, 0553000},
	{"stc1", OPCODE, 0, 0, false, 0554, 0554000},
	{},
	{},
	{"ssdp", OPCODE, 0, 0, false, 0557, 0557000},
    // 560 - 577
	{"rpd",  OPCODERPT, 0, 0, false, 0560, 0560200},     // A = B = C == 1
  	{"rpdx", OPCODERPT, 0, 0, false, 0560, 0560200},     // A = B = C == 0
   	{"rpda", OPCODERPT, 0, 0, false, 0560, 0560200},     // A = C == 1, B == 0
  	{"rpdb", OPCODERPT, 0, 0, false, 0560, 0560200},     // A == 0, C = C == 1
	{},
	{},
	{},
	{},
	{"fdv", OPCODE, 0, 0, false, 0565, 0565000},
	{},
	{"dfdv", OPCODE, 0, 0, false, 0567, 0567000},
	{},
	{},
	{},
	{"fno", OPCODE, 0, 0, false, 0573, 0573000},
	{},
	{"fsb", OPCODE, 0, 0, false, 0575, 0575000},
	{},
	{"dfsb", OPCODE, 0, 0, false, 0577, 0577000},
    // 600 - 617
	{"tze", OPCODE, 0, 0, false, 0600, 0600000},
	{"tnz", OPCODE, 0, 0, false, 0601, 0601000},
	{"tnc", OPCODE, 0, 0, false, 0602, 0602000},
	{"trc", OPCODE, 0, 0, false, 0603, 0603000},
	{"tmi", OPCODE, 0, 0, false, 0604, 0604000},
	{"tpl", OPCODE, 0, 0, false, 0605, 0605000},
	{},
	{"ttf", OPCODE, 0, 0, false, 0607, 0607000},
	{"rtcd", OPCODE, 0, 0, false, 0610, 0610000},
	{},
	{},
	{"rcu", OPCODE, 0, 0, false, 0613, 0613000},
	{"teo", OPCODE, 0, 0, false, 0614, 0614000},
	{"teu", OPCODE, 0, 0, false, 0615, 0615000},
	{"dis", OPCODE, 0, 0, false, 0616, 0616000},
	{"tov", OPCODE, 0, 0, false, 0617, 0617000},
    // 620 - 637
	{"eax0", OPCODE, 0, 0, false, 0620, 0620000},
	{"eaxap", OPCODE, 0, 0, false, 0620, 0620000},

	{"eax1", OPCODE, 0, 0, false, 0621, 0621000},
	{"eaxab", OPCODE, 0, 0, false, 0621, 0621000},

	{"eax2", OPCODE, 0, 0, false, 0622, 0622000},
	{"eaxbp", OPCODE, 0, 0, false, 0622, 0622000},

	{"eax3", OPCODE, 0, 0, false, 0623, 0623000},
	{"eaxbb", OPCODE, 0, 0, false, 0623, 0623000},

	{"eax4", OPCODE, 0, 0, false, 0624, 0624000},
	{"eaxlp", OPCODE, 0, 0, false, 0624, 0624000},

	{"eax5", OPCODE, 0, 0, false, 0625, 0625000},
	{"eaxlb", OPCODE, 0, 0, false, 0625, 0625000},

	{"eax6", OPCODE, 0, 0, false, 0626, 0626000},
	{"eaxsp", OPCODE, 0, 0, false, 0626, 0626000},

	{"eax7", OPCODE, 0, 0, false, 0627, 0627000},
	{"eaxsb", OPCODE, 0, 0, false, 0627, 0627000},
    
	{"ret", OPCODE, 0, 0, false, 0630, 0630000},
	{},
	{},
	{"rccl", OPCODE, 0, 0, false, 0633, 0633000},
	{"ldi", OPCODE, 0, 0, false, 0634, 0634000},
	{"eaa", OPCODE, 0, 0, false, 0635, 0635000},
	{"eaq", OPCODE, 0, 0, false, 0636, 0636000},
	{"ldt", OPCODE, 0, 0, false, 0637, 0637000},
    // 640 - 657
	{"ersx0", OPCODE, 0, 0, false, 0640, 0640000},
	{"ersxap", OPCODE, 0, 0, false, 0640, 0640000},

	{"ersx1", OPCODE, 0, 0, false, 0641, 0641000},
	{"ersxab", OPCODE, 0, 0, false, 0641, 0641000},

	{"ersx2", OPCODE, 0, 0, false, 0642, 0642000},
	{"ersxbp", OPCODE, 0, 0, false, 0642, 0642000},

	{"ersx3", OPCODE, 0, 0, false, 0643, 0643000},
	{"ersxbb", OPCODE, 0, 0, false, 0643, 0643000},

	{"ersx4", OPCODE, 0, 0, false, 0644, 0644000},
	{"ersxlp", OPCODE, 0, 0, false, 0644, 0644000},

	{"ersx5", OPCODE, 0, 0, false, 0645, 0645000},
	{"ersxlb", OPCODE, 0, 0, false, 0645, 0645000},

	{"ersx6", OPCODE, 0, 0, false, 0646, 0646000},
	{"ersxsp", OPCODE, 0, 0, false, 0646, 0646000},

	{"ersx7", OPCODE, 0, 0, false, 0647, 0647000},
    {"ersxsb", OPCODE, 0, 0, false, 0647, 0647000},
    
	{"spri4", OPCODE, 0, 0, false, 0650, 0650000},
	{"sprilp", OPCODE, 0, 0, false, 0650, 0650000},
	
    {"spbp5", OPCODE, 0, 0, false, 0651, 0651000},
    {"spbplb", OPCODE, 0, 0, false, 0651, 0651000},

	{"spri6", OPCODE, 0, 0, false, 0652, 0652000},
	{"sprisp", OPCODE, 0, 0, false, 0652, 0652000},

	{"spbp7", OPCODE, 0, 0, false, 0653, 0653000},
	{"spbpsb", OPCODE, 0, 0, false, 0653, 0653000},
    
	{"stacq", OPCODE, 0, 0, false, 0654, 0654000},
	{"ersa", OPCODE, 0, 0, false, 0655, 0655000},
	{"ersq", OPCODE, 0, 0, false, 0656, 0656000},
	{"scu", OPCODE, 0, 0, false, 0657, 0657000},
    // 660 - 677
	{"erx0", OPCODE, 0, 0, false, 0660, 0660000},
	{"erxap", OPCODE, 0, 0, false, 0660, 0660000},

	{"erx1", OPCODE, 0, 0, false, 0661, 0661000},
	{"erxab", OPCODE, 0, 0, false, 0661, 0661000},

	{"erx2", OPCODE, 0, 0, false, 0662, 0662000},
	{"erxbp", OPCODE, 0, 0, false, 0662, 0662000},

	{"erx3", OPCODE, 0, 0, false, 0663, 0663000},
	{"erxbb", OPCODE, 0, 0, false, 0663, 0663000},

	{"erx4", OPCODE, 0, 0, false, 0664, 0664000},
	{"erxlp", OPCODE, 0, 0, false, 0664, 0664000},

	{"erx5", OPCODE, 0, 0, false, 0665, 0665000},
	{"erxlb", OPCODE, 0, 0, false, 0665, 0665000},

	{"erx6", OPCODE, 0, 0, false, 0666, 0666000},
	{"erxsp", OPCODE, 0, 0, false, 0666, 0666000},

	{"erx7", OPCODE, 0, 0, false, 0667, 0667000},
	{"erxsb", OPCODE, 0, 0, false, 0667, 0667000},
    
	{"tsp4", OPCODE, 0, 0, false, 0670, 0670000},
	{"tsplp", OPCODE, 0, 0, false, 0670, 0670000},
	
    {"tsp5", OPCODE, 0, 0, false, 0671, 0671000},
    {"tsplb", OPCODE, 0, 0, false, 0671, 0671000},

	{"tsp6", OPCODE, 0, 0, false, 0672, 0672000},
	{"tspsp", OPCODE, 0, 0, false, 0672, 0672000},

	{"tsp7", OPCODE, 0, 0, false, 0673, 0673000},
	{"tspsb", OPCODE, 0, 0, false, 0673, 0673000},
    
	{"lcpr", OPCODE, 0, 0, false, 0674, 0674000},
	{"era", OPCODE, 0, 0, false, 0675, 0675000},
	{"erq", OPCODE, 0, 0, false, 0676, 0676000},
	{"eraq", OPCODE, 0, 0, false, 0677, 0677000},
    // 700 - 717
	{"tsx0", OPCODE, 0, 0, false, 0700, 0700000},
	{"tsxap", OPCODE, 0, 0, false, 0700, 0700000},

	{"tsx1", OPCODE, 0, 0, false, 0701, 0701000},
	{"tsxab", OPCODE, 0, 0, false, 0701, 0701000},

	{"tsx2", OPCODE, 0, 0, false, 0702, 0702000},
	{"tsxbp", OPCODE, 0, 0, false, 0702, 0702000},

	{"tsx3", OPCODE, 0, 0, false, 0703, 0703000},
	{"tsxbb", OPCODE, 0, 0, false, 0703, 0703000},

	{"tsx4", OPCODE, 0, 0, false, 0704, 0704000},
	{"tsxlp", OPCODE, 0, 0, false, 0704, 0704000},

	{"tsx5", OPCODE, 0, 0, false, 0705, 0705000},
	{"tsxlb", OPCODE, 0, 0, false, 0705, 0705000},

	{"tsx6", OPCODE, 0, 0, false, 0706, 0706000},
	{"tsxsp", OPCODE, 0, 0, false, 0706, 0706000},

	{"tsx7", OPCODE, 0, 0, false, 0707, 0707000},
	{"tsxsb", OPCODE, 0, 0, false, 0707, 0707000},
    
	{"tra", OPCODE, 0, 0, false, 0710, 0710000},
	{},
	{},
	{"call6", OPCODE, 0, 0, false, 0713, 0713000},
	{},
	{"tss", OPCODE, 0, 0, false, 0715, 0715000},
	{"xec", OPCODE, 0, 0, false, 0716, 0716000},
	{"xed", OPCODE, 0, 0, false, 0717, 0717000},
    // 720 - 737
	{"lxl0", OPCODE, 0, 0, false, 0720, 0720000},
	{"lxlap", OPCODE, 0, 0, false, 0720, 0720000},

	{"lxl1", OPCODE, 0, 0, false, 0721, 0721000},
	{"lxlab", OPCODE, 0, 0, false, 0721, 0721000},

	{"lxl2", OPCODE, 0, 0, false, 0722, 0722000},
	{"lxlbp", OPCODE, 0, 0, false, 0722, 0722000},

	{"lxl3", OPCODE, 0, 0, false, 0723, 0723000},
	{"lxlbb", OPCODE, 0, 0, false, 0723, 0723000},

	{"lxl4", OPCODE, 0, 0, false, 0724, 0724000},
	{"lxllp", OPCODE, 0, 0, false, 0724, 0724000},

	{"lxl5", OPCODE, 0, 0, false, 0725, 0725000},
	{"lxllb", OPCODE, 0, 0, false, 0725, 0725000},

	{"lxl6", OPCODE, 0, 0, false, 0726, 0726000},
	{"lxlsp", OPCODE, 0, 0, false, 0726, 0726000},

	{"lxl7", OPCODE, 0, 0, false, 0727, 0727000},
	{"lxlsb", OPCODE, 0, 0, false, 0727, 0727000},

	{},
	{"ars", OPCODE, 0, 0, false, 0731, 0731000},
	{"qrs", OPCODE, 0, 0, false, 0732, 0732000},
	{"lrs", OPCODE, 0, 0, false, 0733, 0733000},
	{},
	{"als", OPCODE, 0, 0, false, 0735, 0735000},
	{"qls", OPCODE, 0, 0, false, 0736, 0736000},
	{"lls", OPCODE, 0, 0, false, 0737, 0737000},
    // 740 - 757
	{"stx0", OPCODE, 0, 0, false, 0740, 0740000},
	{"stxap", OPCODE, 0, 0, false, 0740, 0740000},

	{"stx1", OPCODE, 0, 0, false, 0741, 0741000},
	{"stxab", OPCODE, 0, 0, false, 0741, 0741000},

	{"stx2", OPCODE, 0, 0, false, 0742, 0742000},
	{"stxbp", OPCODE, 0, 0, false, 0742, 0742000},

	{"stx3", OPCODE, 0, 0, false, 0743, 0743000},
	{"stxbb", OPCODE, 0, 0, false, 0743, 0743000},

	{"stx4", OPCODE, 0, 0, false, 0744, 0744000},
	{"stxlp", OPCODE, 0, 0, false, 0744, 0744000},

	{"stx5", OPCODE, 0, 0, false, 0745, 0745000},
	{"stxlb", OPCODE, 0, 0, false, 0745, 0745000},

	{"stx6", OPCODE, 0, 0, false, 0746, 0746000},
	{"stxsp", OPCODE, 0, 0, false, 0746, 0746000},

	{"stx7", OPCODE, 0, 0, false, 0747, 0747000},
	{"stxsb", OPCODE, 0, 0, false, 0747, 0747000},
    
	{"stc2", OPCODE, 0, 0, false, 0750, 0750000},
	{"stca", OPCODESTC, 0, 0, false, 0751, 0751000},
	{"stcq", OPCODESTC, 0, 0, false, 0752, 0752000},
	{"sreg", OPCODE, 0, 0, false, 0753, 0753000},
	{"sti", OPCODE, 0, 0, false, 0754, 0754000},
	{"sta", OPCODE, 0, 0, false, 0755, 0755000},
	{"stq", OPCODE, 0, 0, false, 0756, 0756000},
	{"staq", OPCODE, 0, 0, false, 0757, 0757000},
    // 760 - 777
	{"lprp0", OPCODE, 0, 0, false, 0760, 0760000},
	{"lprpap", OPCODE, 0, 0, false, 0760, 0760000},

	{"lprp1", OPCODE, 0, 0, false, 0761, 0761000},
	{"lprpab", OPCODE, 0, 0, false, 0761, 0761000},

	{"lprp2", OPCODE, 0, 0, false, 0762, 0762000},
	{"lprpbp", OPCODE, 0, 0, false, 0762, 0762000},

	{"lprp3", OPCODE, 0, 0, false, 0763, 0763000},
	{"lprpbb", OPCODE, 0, 0, false, 0763, 0763000},

	{"lprp4", OPCODE, 0, 0, false, 0764, 0764000},
	{"lprplp", OPCODE, 0, 0, false, 0764, 0764000},

	{"lprp5", OPCODE, 0, 0, false, 0765, 0765000},
	{"lprplb", OPCODE, 0, 0, false, 0765, 0765000},

	{"lprp6", OPCODE, 0, 0, false, 0766, 0766000},
	{"lprpsp", OPCODE, 0, 0, false, 0766, 0766000},

	{"lprp7", OPCODE, 0, 0, false, 0767, 0767000},
	{"lprpsb", OPCODE, 0, 0, false, 0767, 0767000},
    
	{},
	{"arl", OPCODE, 0, 0, false, 0771, 0771000},
	{"qrl", OPCODE, 0, 0, false, 0772, 0772000},
	{"lrl", OPCODE, 0, 0, false, 0773, 0773000},
	{"gtb", OPCODE, 0, 0, false, 0774, 0774000},
	{"alr", OPCODE, 0, 0, false, 0775, 0775000},
	{"qlr", OPCODE, 0, 0, false, 0776, 0776000},
	{"llr", OPCODE, 0, 0, false, 0777, 0777000},
    
    // EIS....
    // 000 - 017
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 020 - 037
	{"mve", OPCODEMW, 0, 3, true, 0020, 0020400},
	{},
	{},
	{},
	{"mvne", OPCODEMW, 0, 3, true, 0024, 0024400},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 040 - 057
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 060 - 077
	{"csl", OPCODEMW, 0, 2, true, 0060, 0060400},
	{"csr", OPCODEMW, 0, 2, true, 0061, 0061400},
	{},
	{},
	{"sztl", OPCODEMW, 0, 2, true, 0064, 0064400},
	{"sztr", OPCODEMW, 0, 2, true, 0065, 0065400},
	{"cmpb", OPCODEMW, 0, 2, true, 0066, 0066400},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 100 - 117
	{"mlr", OPCODEMW, 0, 2, true, 0100, 0100400},
	{"mrl", OPCODEMW, 0, 2, true, 0101, 0101400},
	{},
	{},
	{},
	{},
	{"cmpc", OPCODEMW, 0, 2, true, 0106, 0106400},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 120 - 137
	{"scd", OPCODEMW, 0, 3, true, 0120, 0120400},
	{"scdr", OPCODEMW, 0, 3, true, 0121, 0121400},
	{},
	{},
	{"scm", OPCODEMW, 0, 3, true, 0124, 0124400},
	{"scmr", OPCODEMW, 0, 3, true, 0125, 0125400},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 140 - 157
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{"sptr", OPCODE, 0, 0, true, 0154, 0154400},
	{},
	{},
	{},
    // 160 - 177
	{"mvt", OPCODEMW, 0, 3, true, 0160, 0160400},
	{},
	{},
	{},
	{"tct",  OPCODEMW, 0, 3, true, 0164, 0164400},
	{"tctr", OPCODEMW, 0, 3, true, 0165, 0165400},
	{},
	{},
	{},
	{},
	{},
	{"lptr", OPCODE, 0, 0, true, 0173, 0173400},
	{},
	{},
	{},
	{},
    // 200 - 217
	{},
	{},
	{"ad2d", OPCODEMW, 0, 2, true, 0202, 0202400},
	{"sb2d", OPCODEMW, 0, 2, true, 0203, 0203400},
	{},
	{},
	{"mp2d", OPCODEMW, 0, 2, true, 0206, 0206400},
	{"dv2d", OPCODEMW, 0, 2, true, 0207, 0207400},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 220 - 237
	{},
	{},
	{"ad3d", OPCODEMW, 0, 3, true, 0222, 0222400},
	{"sb3d", OPCODEMW, 0, 3, true, 0223, 0223400},
	{},
	{},
	{"mp3d", OPCODEMW, 0, 3, true, 0226, 0226400},
	{"dv3d", OPCODEMW, 0, 3, true, 0227, 0227400},
	{},
	{},
	{"lsdr", OPCODE, 0, 0, true, 0232, 0232400},
	{},
	{},
	{},
	{},
	{},
    // 240 - 257
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{"spbp0", OPCODE, 0, 0, true, 0250, 0250400},
	{"spbpap", OPCODE, 0, 0, true, 0250, 0250400},

	{"spri1", OPCODE, 0, 0, true, 0251, 0251400},
    {"spriab", OPCODE, 0, 0, true, 0251, 0251400},
    
	{"spbp2", OPCODE, 0, 0, true, 0252, 0252400},
	{"spbpbp", OPCODE, 0, 0, true, 0252, 0252400},

	{"spri3", OPCODE, 0, 0, true, 0253, 0253400},
	{"spribb", OPCODE, 0, 0, true, 0253, 0253400},

	{"ssdr", OPCODE, 0, 0, true, 0254, 0254400},
	{},
	{},
	{"lptp", OPCODE, 0, 0, true, 0257, 0257400},
    // 260 - 277
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 300 - 317
	{"mvn", OPCODEMW, 0, 3, true, 0300, 0300400},
	{"btd", OPCODEMW, 0, 2, true, 0301, 0301400},
	{},
	{"cmpn", OPCODEMW, 0, 2, true, 0303, 0303400},
	{},
	{"dtb", OPCODEMW, 0, 2, true, 0305, 0305400},
	{},
	{},
	{"easp1", OPCODE, 0, 0, true, 0310, 0310400},
    {"easpab", OPCODE, 0, 0, true, 0310, 0310400},
    
	{"eawp1", OPCODE, 0, 0, true, 0311, 0311400},
    {"eawpab", OPCODE, 0, 0, true, 0311, 0311400},
    
	{"easp3", OPCODE, 0, 0, true, 0312, 0312400},
    {"easpbb", OPCODE, 0, 0, true, 0312, 0312400},
    
	{"eawp3", OPCODE, 0, 0, true, 0313, 0313400},
	{"eawpbb", OPCODE, 0, 0, true, 0313, 0313400},

	{},
	{},
	{},
	{},
    // 320 - 337
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{"easp5", OPCODE, 0, 0, true, 0330, 0330400},
	{"easplb", OPCODE, 0, 0, true, 0330, 0330400},
    
	{"eawp5", OPCODE, 0, 0, true, 0331, 0331400},
   	{"eawplb", OPCODE, 0, 0, true, 0331, 0331400},
    
	{"easp7", OPCODE, 0, 0, true, 0332, 0332400},
	{"easpsb", OPCODE, 0, 0, true, 0332, 0332400},

	{"eawp7", OPCODE, 0, 0, true, 0333, 0333400},
	{"eawpsb", OPCODE, 0, 0, true, 0333, 0333400},

	{},
	{},
	{},
	{},
    // 340 - 357
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{"epbp0", OPCODE, 0, 0, true, 0350, 0350400},
	{"epbpap", OPCODE, 0, 0, true, 0350, 0350400},

	{"epp1",  OPCODE, 0, 0, true, 0351, 0351400},
	{"eppab",  OPCODE, 0, 0, true, 0351, 0351400},

	{"epbp2", OPCODE, 0, 0, true, 0352, 0352400},
	{"epbpbp", OPCODE, 0, 0, true, 0352, 0352400},

	{"epp3",  OPCODE, 0, 0, true, 0353, 0353400},
	{"eppbb",  OPCODE, 0, 0, true, 0353, 0353400},

	{},
	{},
	{},
	{},
    // 360 - 377
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{"epbp4", OPCODE, 0, 0, true, 0370, 0370400},
	{"epbplp", OPCODE, 0, 0, true, 0370, 0370400},

	{"epp5",  OPCODE, 0, 0, true, 0371, 0371400},
	{"epplb",  OPCODE, 0, 0, true, 0371, 0371400},

	{"epbp6", OPCODE, 0, 0, true, 0372, 0372400},
	{"epbpsp", OPCODE, 0, 0, true, 0372, 0372400},

	{"epp7",  OPCODE, 0, 0, true, 0373, 0373400},
	{"eppsb",  OPCODE, 0, 0, true, 0373, 0373400},

	{},
	{},
	{},
	{},
    // 400 - 417
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 420 - 437
	{"emcall", OPCODE, 0, 0, true, 0420, 0420400},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 440 - 457
	{},
	{},
	{},
	{"sareg", OPCODE, 0, 0, true, 0443, 0443400},
	{},
	{},
	{},
	{"spl", OPCODE, 0, 0, true, 0447, 0447400},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 460 - 477
	{},
	{},
	{},
	{"lareg", OPCODE, 0, 0, true, 0463, 0463400},
	{},
	{},
	{},
	{"lpl", OPCODE, 0, 0, true, 0467, 0467400},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 500 - 517
	{"a9bd", OPCODEARS, 0, 0, true, 0500, 0500500},
	{"a6bd", OPCODEARS, 0, 0, true, 0501, 0501500},
	{"a4bd", OPCODEARS, 0, 0, true, 0502, 0502500},
	{"abd",  OPCODEARS, 0, 0, true, 0503, 0503500},
	{},
	{},
	{},
	{"awd", OPCODEARS, 0, 0, true, 0507, 0507500},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 520 - 537
	{"s9bd", OPCODEARS, 0, 0, true, 0520, 0520500},
	{"s6bd", OPCODEARS, 0, 0, true, 0521, 0521500},
	{"s4bd", OPCODEARS, 0, 0, true, 0522, 0522500},
	{"sbd",  OPCODEARS, 0, 0, true, 0523, 0523500},
	{},
	{},
	{},
	{"swd", OPCODEARS, 0, 0, true, 0527, 0527500},
	{},
	{},
	{"camp", OPCODE, 0, 0, true, 0532, 0532400},
	{},
	{},
	{},
	{},
	{},
    // 540 - 557
	{"ara0", OPCODE, 0, 0, true, 0540, 0540400},
	{"araap", OPCODE, 0, 0, true, 0540, 0540400},

	{"ara1", OPCODE, 0, 0, true, 0541, 0541400},
	{"araab", OPCODE, 0, 0, true, 0541, 0541400},

	{"ara2", OPCODE, 0, 0, true, 0542, 0542400},
	{"arabp", OPCODE, 0, 0, true, 0542, 0542400},

	{"ara3", OPCODE, 0, 0, true, 0543, 0543400},
	{"arabb", OPCODE, 0, 0, true, 0543, 0543400},

	{"ara4", OPCODE, 0, 0, true, 0544, 0544400},
	{"aralp", OPCODE, 0, 0, true, 0544, 0544400},

	{"ara5", OPCODE, 0, 0, true, 0545, 0545400},
	{"aralb", OPCODE, 0, 0, true, 0545, 0545400},

	{"ara6", OPCODE, 0, 0, true, 0546, 0546400},
	{"arasp", OPCODE, 0, 0, true, 0546, 0546400},

	{"ara7", OPCODE, 0, 0, true, 0547, 0547400},
	{"arasb", OPCODE, 0, 0, true, 0547, 0547400},

	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{"sptp", OPCODE, 0, 0, true, 0557, 0557400},
    // 560 - 577
	{"aar0", OPCODE, 0, 0, true, 0560, 0560400},
	{"aarap", OPCODE, 0, 0, true, 0560, 0560400},

	{"aar1", OPCODE, 0, 0, true, 0561, 0561400},
	{"aarab", OPCODE, 0, 0, true, 0561, 0561400},

	{"aar2", OPCODE, 0, 0, true, 0562, 0562400},
	{"aarbp", OPCODE, 0, 0, true, 0562, 0562400},

	{"aar3", OPCODE, 0, 0, true, 0563, 0563400},
	{"aarbb", OPCODE, 0, 0, true, 0563, 0563400},

	{"aar4", OPCODE, 0, 0, true, 0564, 0564400},
	{"aarlp", OPCODE, 0, 0, true, 0564, 0564400},

	{"aar5", OPCODE, 0, 0, true, 0565, 0565400},
	{"aarlb", OPCODE, 0, 0, true, 0565, 0565400},

	{"aar6", OPCODE, 0, 0, true, 0566, 0566400},
	{"aarsp", OPCODE, 0, 0, true, 0566, 0566400},

	{"aar7", OPCODE, 0, 0, true, 0567, 0567400},
	{"aarsb", OPCODE, 0, 0, true, 0567, 0567400},

	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 600 - 617
	{"trtn", OPCODE, 0, 0, true, 0600, 0600400},
	{"trtf", OPCODE, 0, 0, true, 0601, 0601400},
	{},
	{},
	{"tmoz", OPCODE, 0, 0, true, 0604, 0604400},
	{"tpnz", OPCODE, 0, 0, true, 0605, 0605400},
	{"ttn",  OPCODE, 0, 0, true, 0606, 0606400},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 620 - 637
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 640 - 657
	{"arn0", OPCODE, 0, 0, true, 0640, 0640400},
    {"arnap", OPCODE, 0, 0, true, 0640, 0640400},
    
	{"arn1", OPCODE, 0, 0, true, 0641, 0641400},
	{"arnab", OPCODE, 0, 0, true, 0641, 0641400},

	{"arn2", OPCODE, 0, 0, true, 0642, 0642400},
	{"arnbp", OPCODE, 0, 0, true, 0642, 0642400},

	{"arn3", OPCODE, 0, 0, true, 0643, 0643400},
	{"arnbb", OPCODE, 0, 0, true, 0643, 0643400},

	{"arn4", OPCODE, 0, 0, true, 0644, 0644400},
	{"arnlp", OPCODE, 0, 0, true, 0644, 0644400},

	{"arn5", OPCODE, 0, 0, true, 0645, 0645400},
	{"arnlb", OPCODE, 0, 0, true, 0645, 0645400},

	{"arn6", OPCODE, 0, 0, true, 0646, 0646400},
	{"arnsp", OPCODE, 0, 0, true, 0646, 0646400},

	{"arn7", OPCODE, 0, 0, true, 0647, 0647400},
	{"arnsb", OPCODE, 0, 0, true, 0647, 0647400},
    
	{"spbp4", OPCODE, 0, 0, true, 0650, 0650400},
	{"spbplp", OPCODE, 0, 0, true, 0650, 0650400},
	
    {"spri5", OPCODE, 0, 0, true, 0651, 0651400},
    {"sprilb", OPCODE, 0, 0, true, 0651, 0651400},

	{"spbp6", OPCODE, 0, 0, true, 0652, 0652400},
	{"spbpsp", OPCODE, 0, 0, true, 0652, 0652400},

	{"spri7", OPCODE, 0, 0, true, 0653, 0653400},
	{"sprisb", OPCODE, 0, 0, true, 0653, 0653400},

	{},
	{},
	{},
	{},
    // 660 - 677
	{"nar0", OPCODE, 0, 0, true, 0660, 0660400},
    {"narap", OPCODE, 0, 0, true, 0660, 0660400},
    
	{"nar1", OPCODE, 0, 0, true, 0661, 0661400},
	{"narab", OPCODE, 0, 0, true, 0661, 0661400},

	{"nar2", OPCODE, 0, 0, true, 0662, 0662400},
	{"narbp", OPCODE, 0, 0, true, 0662, 0662400},

	{"nar3", OPCODE, 0, 0, true, 0663, 0663400},
	{"narbb", OPCODE, 0, 0, true, 0663, 0663400},

	{"nar4", OPCODE, 0, 0, true, 0664, 0664400},
	{"narlp", OPCODE, 0, 0, true, 0664, 0664400},

	{"nar5", OPCODE, 0, 0, true, 0665, 0665400},
	{"narlb", OPCODE, 0, 0, true, 0665, 0665400},

	{"nar6", OPCODE, 0, 0, true, 0666, 0666400},
	{"narsp", OPCODE, 0, 0, true, 0666, 0666400},

	{"nar7", OPCODE, 0, 0, true, 0667, 0667400},
	{"narsb", OPCODE, 0, 0, true, 0667, 0667400},

	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 700 - 717
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 720 - 737
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
    // 740 - 757
	{"sar0", OPCODE, 0, 0, true, 0740, 0740400},
	{"sarap", OPCODE, 0, 0, true, 0740, 0740400},

	{"sar1", OPCODE, 0, 0, true, 0741, 0741400},
	{"sarab", OPCODE, 0, 0, true, 0741, 0741400},

	{"sar2", OPCODE, 0, 0, true, 0742, 0742400},
	{"sarbp", OPCODE, 0, 0, true, 0742, 0742400},

	{"sar3", OPCODE, 0, 0, true, 0743, 0743400},
	{"sarbb", OPCODE, 0, 0, true, 0743, 0743400},

	{"sar4", OPCODE, 0, 0, true, 0744, 0744400},
	{"sarlp", OPCODE, 0, 0, true, 0744, 0744400},

	{"sar5", OPCODE, 0, 0, true, 0745, 0745400},
	{"sarlb", OPCODE, 0, 0, true, 0745, 0745400},

	{"sar6", OPCODE, 0, 0, true, 0746, 0746400},
	{"sarsp", OPCODE, 0, 0, true, 0746, 0746400},

	{"sar7", OPCODE, 0, 0, true, 0747, 0747400},
	{"sarsb", OPCODE, 0, 0, true, 0747, 0747400},

	{},
	{},
	{},
	{},
	{"sra", OPCODE, 0, 0, true, 0754, 0754400},
	{},
	{},
	{},
    // 760 - 777
	{"lar0", OPCODE, 0, 0, true, 0760, 0760400},
	{"larap", OPCODE, 0, 0, true, 0760, 0760400},

	{"lar1", OPCODE, 0, 0, true, 0761, 0761400},
    {"larab", OPCODE, 0, 0, true, 0761, 0761400},
    
	{"lar2", OPCODE, 0, 0, true, 0762, 0762400},
	{"larbp", OPCODE, 0, 0, true, 0762, 0762400},

	{"lar3", OPCODE, 0, 0, true, 0763, 0763400},
	{"larbb", OPCODE, 0, 0, true, 0763, 0763400},

	{"lar4", OPCODE, 0, 0, true, 0764, 0764400},
	{"larlp", OPCODE, 0, 0, true, 0764, 0764400},

	{"lar5", OPCODE, 0, 0, true, 0765, 0765400},
	{"larlb", OPCODE, 0, 0, true, 0765, 0765400},

	{"lar6", OPCODE, 0, 0, true, 0766, 0766400},
	{"larsp", OPCODE, 0, 0, true, 0766, 0766400},

	{"lar7", OPCODE, 0, 0, true, 0767, 0767400},
	{"larsb", OPCODE, 0, 0, true, 0767, 0767400},

	{},
	{}, 
	{}, 
	{}, 
	{"lra", OPCODE, 0, 0, true, 0774, 0774400},
	{}, 
	{}, 
	{},
    
    // special stuff
    
    // A=0 Address RegisterSpecial
    {"awdx",  OPCODEARS, 0, 0, true, 0507, 0507400},  //Add Word Displacement to Address Register
    {"a4bdx", OPCODEARS, 0, 0, true, 0502, 0502400},  //Add 4-bit Displacement to Address Register
    {"a6bdx", OPCODEARS, 0, 0, true, 0501, 0501400},  //Add 6-bit Displacement to Address Register
    {"a9bdx", OPCODEARS, 0, 0, true, 0500, 0500400},  //Add 9-bit Displacement to Address Register
    {"abdx",  OPCODEARS, 0, 0, true, 0503, 0503400},  //Add  bit Displacement to Address Register

    {"swdx",  OPCODEARS, 0, 0, true, 0527, 0527400},  //Subtract Word Displacement from Address Register
    {"s4bdx", OPCODEARS, 0, 0, true, 0522, 0522400},  //Subtract 4-bit Displacement from Address Register
    {"s6bdx", OPCODEARS, 0, 0, true, 0521, 0521400},  //Subtract 6-bit Displacement from Address Register
    {"s9bdx", OPCODEARS, 0, 0, true, 0520, 0520400},  //Subtract 9-bit Displacement from Address Register
    {"sbdx",  OPCODEARS, 0, 0, true, 0523, 0523400},  //Subtract  bit Displacement from Address Register
};


opCode *InstructionTable = NULL;

struct adrMods extMods[0100] = {    ///< address modifiers w/ extended info
    /* R */
    {NULL, 0},
    {"au", 1},
    {"qu", 2},
    {"du", 3},
    {"ic", 4},
    {"al", 5},
    {"ql", 6},
    {"dl", 7},
    {"0",  8},
    {"1",  9},
    {"2", 10},
    {"3", 11},
    {"4", 12},
    {"5", 13},
    {"6", 14},
    {"7", 15},

    /* RI */
    {"n*",  16},
    {"au*", 17},
    {"qu*", 18},
    {NULL,  19},
    {"ic*", 20},
    {"al*", 21},
    {"ql*", 22},
    {NULL,  23},
    {"0*",  24},
    {"1*",  25},
    {"2*",  26},
    {"3*",  27},
    {"4*",  28},
    {"5*",  29},
    {"6*",  30},
    {"7*",  31},
    
    /* IT */
    {"f1",  32},
    {"itp", 33},
    {NULL,  34},
    {"its", 35},
    {"sd",  36},
    {"scr", 37},
    {"f2",  38},
    {"f3",  39},
    {"ci",  40},
    {"i",   41},
    {"sc",  42},
    {"ad",  43},
    {"di",  44},
    {"dic", 45},
    {"id",  46},
    {"idc", 47},
    
    /* IR */
    {"*n",  48},
    {"*au", 49},
    {"*qu", 50},
    {"*du", 51},
    {"*ic", 52},
    {"*al", 53},
    {"*ql", 54},
    {"*dl", 55},
    {"*0",  56},
    {"*1",  57},
    {"*2",  58},
    {"*3",  59},
    {"*4",  60},
    {"*5",  61},
    {"*6",  62},
    {"*7",  63},
};

char GEBcdToASCII[64] = ///< from pg 271 CPB1004F_GE635pgmRef_Jul69
{
/* 000 - 007 */    '0', '1', '2', '3', '4', '5', '6', '7',
/* 010 - 017 */    '8', '9', '[', '#', '@', ':', '>', '?',
/* 020 - 027 */    ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
/* 030 - 037 */    'H', 'I', '&', '.', ']', '(', '<', '\\',
/* 040 - 047 */    '^', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
/* 050 - 057 */    'Q', 'R', '-', '$', '*', ')', ';', '\'',
/* 060 - 067 */    '+', '/', 'S', 'T', 'U', 'V', 'W', 'X',
/* 070 - 077 */    'Y', 'Z', '_', ',', '%', '=', '"', '!'
};

char ASCIIToGEBcd[128] =
{
/* 000 - 007 */    -1, -1, -1, -1, -1, -1, -1, -1,
/* 010 - 017 */    -1, -1, -1, -1, -1, -1, -1, -1,
/* 020 - 027 */    -1, -1, -1, -1, -1, -1, -1, -1,
/* 030 - 037 */    -1, -1, -1, -1, -1, -1, -1, -1,
/* 040 - 047 */    16, 63, 62, 11, 43, 60, 26, 47,
/* 050 - 057 */    29, 45, 44, 48, 59, 42, 27, 49,
/* 060 - 067 */     0,  1,  2,  3,  4,  5,  6,  7,
/* 070 - 077 */     8,  9, 13, 46, 30, 61, 14, 15,
/* 100 - 107 */    12, 17, 18, 19, 20, 21, 22, 23,
/* 110 - 117 */    24, 25, 33, 34, 35, 36, 37, 38,
/* 120 - 127 */    39, 40, 41, 50, 51, 52, 53, 54,
/* 130 - 137 */    55, 56, 57, 10, 31, 28, 32, 58,
/* 140 - 147 */    -1, 17, 18, 19, 20, 21, 22, 23,
/* 150 - 157 */    24, 25, 33, 34, 35, 36, 37, 38,
/* 160 - 167 */    39, 40, 41, 50, 51, 52, 53, 54,
/* 170 - 177 */    55, 56, 57, -1, -1, -1, -1, -1,
};


void
initInstructionTable()
{
    int nCount = 0;
    
    opCode *src = allOpcodes;
    for(int i = 0 ; i < sizeof(allOpcodes) / sizeof(opCode); i += 1, src += 1)
    {
        if (src->mne)
        {
            opCode *s = NULL;
        
            HASH_FIND_STR(InstructionTable, src->mne, s);  /* s: output pointer */
            if (s)
            {
                fprintf(stderr, "ERROR: instruction <%s> already in table!\n", s->mne);
                exit(1);
            }
                
            HASH_ADD_KEYPTR(hh, InstructionTable, src->mne, strlen(src->mne), src);
     
            nCount += 1;
         }
    }
    if (debug)
        fprintf(stderr, "%d opcodes added to instruction table ...\n", nCount);
}

