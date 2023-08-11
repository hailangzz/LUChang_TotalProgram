#ifndef __DO_PARAM__H_
#define __DO_PARAM__H_

#include "iniparser.h"
#include "dictionary.h"
#include "ini_ctype.h"
#include "vim_type.h"
#include "mpi_dcvi.h"



typedef struct __dcvi_attr_str{
	VIM_U8 u8ChnEn;
	VIM_DCVI_DEV_MODE_S dcvi_dev[DCVI_DEV_MAX];
	VIM_DCVI_CHN_MODE_S dcvi_chn[DCVI_CHN_MAX];
}dcvi_attr_str;

VIM_BOOL SAMPLE_DCVI_GetBoolFromIniBykey( dictionary   * ini,char *keyname);
VIM_U32 SAMPLE_DCVI_GetDcviEnTypeFromIniBykey( dictionary   * ini, char *keyname);
VIM_U32 SAMPLE_DCVI_GetInputFormatFromIniBykey( dictionary   * ini, char *keyname);
VIM_U32 SAMPLE_DCVI_GetScanModeFromIniBykey( dictionary   * ini, char *keyname);


VIM_S32 SAMPLE_DCVI_GetAttrFromIni(VIM_CHAR* cfg, dcvi_attr_str* attr);

#endif

