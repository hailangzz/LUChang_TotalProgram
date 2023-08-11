#include "sample_comm_dcvi.h"


#define ARRAY_SIZE(arr)		(sizeof(arr)/sizeof(arr[0]))

VIM_BOOL SAMPLE_DCVI_GetBoolFromIniBykey( dictionary   * ini,char *keyname)
{
	const char * str = NULL;
	int i = 0;
	char * boolStr[] = {
		"VIM_FALSE",
		"VIM_TRUE"
	};
	
	str = iniparser_getstring(ini, keyname, NULL);
	if (str == NULL)
		return VIM_FALSE;
	
	
	for (i = 0;  i < ARRAY_SIZE(boolStr); i++)
	{
		if (strncmp(str, boolStr[i], strlen(boolStr[i])) == 0)
		{
			return i;
		}
	}
	
	return VIM_FALSE;
}


VIM_U32 SAMPLE_DCVI_GetInputFormatFromIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	int i = 0;
	char * inputTypeStr[] = {
		"E_INPUT_FORMAT_422SP",
		"E_INPUT_FORMAT_420SP",
		"E_INPUT_FORMAT_422P",
		"E_INPUT_FORMAT_420P",
		"E_INPUT_FORMAT_601P",
		"E_INPUT_FORMAT_709P",
		"E_INPUT_FORMAT_UYVY422I",
	};
	
	str = iniparser_getstring(ini, keyname, NULL);
	if (str == NULL)
		return 3;
	
	
	for (i = 0;  i < ARRAY_SIZE(inputTypeStr); i++)
	{
		if (strncmp(str, inputTypeStr[i], strlen(inputTypeStr[i])) == 0)
		{
			return i;
		}
	}
	
	return 3;
}

VIM_U32 SAMPLE_DCVI_GetScanModeFromIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	int i = 0;
	char * inputTypeStr[] = {
		"E_DCVI_SCAN_MODE_PROGRESSIVE",
		"E_DCVI_SCAN_MODE_INTERLACE",
	};
	
	str = iniparser_getstring(ini, keyname, NULL);
	if (str == NULL)
		return 0;
	
	for (i = 0;  i < ARRAY_SIZE(inputTypeStr); i++)
	{
		if (strncmp(str, inputTypeStr[i], strlen(inputTypeStr[i])) == 0)
		{
			
			return i;
		}
	}
	
	return 0;
}

VIM_U32 SAMPLE_DCVI_GetDcviEnTypeFromIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	int i = 0;
	char * inputTypeStr[] = {
		"E_DCVI_INPUT_TYPE_BT656",
		"E_DCVI_INPUT_TYPE_BT1120",
		"E_DCVI_INPUT_TYPE_BT1120_2WAY",
	};
	
	str = iniparser_getstring(ini, keyname, NULL);
	if (str == NULL)
		return 0;
	
	for (i = 0;  i < ARRAY_SIZE(inputTypeStr); i++)
	{
		if (strlen(inputTypeStr[i]) == strlen(str))
		if (strncmp(str, inputTypeStr[i], strlen(inputTypeStr[i])) == 0)
		{
			
			return i;
		}
	}
	
	return 0;
}

VIM_S32 SAMPLE_DCVI_GetAttrFromIni(VIM_CHAR* cfg, dcvi_attr_str* attr)
{
	char keyname[128] = {0};
	VIM_U32 u32TmpChn = 0;
	dictionary* ini = NULL;

	ini = iniparser_load(cfg);
	if(ini == NULL){
		printf("ini open error !!! \r\n");
		return -1;
	}

	for(u32TmpChn = 0; u32TmpChn < DCVI_DEV_MAX; u32TmpChn++){
		sprintf(keyname, "DCVI_DEV_ATTR:enType[%d]",u32TmpChn);			
		attr->dcvi_dev[u32TmpChn].eInputType = SAMPLE_DCVI_GetDcviEnTypeFromIniBykey(ini, keyname);

		sprintf(keyname, "DCVI_DEV_ATTR:bTestBarEn[%d]",u32TmpChn);
		attr->dcvi_dev[u32TmpChn].bTestBarEn = SAMPLE_DCVI_GetBoolFromIniBykey(ini, keyname);

		sprintf(keyname, "DCVI_DEV_ATTR:u8TestColorSel[%d]",u32TmpChn);		
		attr->dcvi_dev[u32TmpChn].eColorType = iniparser_getint(ini, keyname, 8);

	}
	for ( u32TmpChn = 0; u32TmpChn < MAX_DCVI_CHN; u32TmpChn++ ){	
		sprintf(keyname,"DCVI_CHN%d:bEnable",u32TmpChn);
		attr->u8ChnEn |= SAMPLE_DCVI_GetBoolFromIniBykey(ini, keyname)<<u32TmpChn;
		sprintf(keyname, "DCVI_CHN%d:enFormat", u32TmpChn);
		attr->dcvi_chn[u32TmpChn].eFormat = SAMPLE_DCVI_GetInputFormatFromIniBykey(ini, keyname);
		sprintf(keyname, "DCVI_CHN%d:bYcbcr", u32TmpChn);
		attr->dcvi_chn[u32TmpChn].bYcbcr = SAMPLE_DCVI_GetBoolFromIniBykey(ini, keyname);
		sprintf(keyname, "DCVI_CHN%d:bSeqUV", u32TmpChn);
		attr->dcvi_chn[u32TmpChn].eSeqUV = SAMPLE_DCVI_GetBoolFromIniBykey(ini, keyname);
		sprintf(keyname, "DCVI_CHN%d:enScanMode", u32TmpChn);
		attr->dcvi_chn[u32TmpChn].enScanMode = SAMPLE_DCVI_GetScanModeFromIniBykey(ini, keyname);
		
		sprintf(keyname, "DCVI_CHN%d:u32CropLeft", u32TmpChn);			
		attr->dcvi_chn[u32TmpChn].u32CropLeft = iniparser_getint(ini, keyname, 10);
		sprintf(keyname, "DCVI_CHN%d:u32CropTop", u32TmpChn);
		attr->dcvi_chn[u32TmpChn].u32CropTop = iniparser_getint(ini, keyname, 10);
		sprintf(keyname, "DCVI_CHN%d:u32CropWidth", u32TmpChn);
		attr->dcvi_chn[u32TmpChn].u32CropWidth  = iniparser_getint(ini, keyname, 640);
		sprintf(keyname, "DCVI_CHN%d:u32CropHeight", u32TmpChn);
		attr->dcvi_chn[u32TmpChn].u32CropHeight = iniparser_getint(ini, keyname, 480);
		
		sprintf(keyname, "DCVI_CHN%d:u32Fps", u32TmpChn);
		attr->dcvi_chn[u32TmpChn].u32Fps = iniparser_getint(ini, keyname, 30);		
		
	}
	
	return 0;
}