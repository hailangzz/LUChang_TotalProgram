#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "iniparser.h"
#include "dictionary.h"
#include "ini_ctype.h"
#include "mpi_rotation.h"
#include "sample_common_rotation.h"
#include "sample_common_rotation_param.h"

extern ROT_SAMPLE_MAN_INFO_S     ROT_mstManInfo;
extern VIM_ROFORMAT_S ChnpAttr[16];

static VIM_BOOL __GetBoolFromIniBykey( dictionary   * ini,char *keyname)
{
	const char * str = NULL;
	VIM_U32 i = 0;
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

static VIM_U32 __GetRotBitFromIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	VIM_U32 i = 0;
	char * inputTypeStr[] = {
		"VIM_ROTATION_PIX_BIT_10",
		"VIM_ROTATION_PIX_BIT_8",
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

static VIM_U32 __GetRotModIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	VIM_U32 i = 0;
	char * inputTypeStr[] = {
		"VIM_ROTATION_MOD_LEFT"  ,
		"VIM_ROTATION_MOD_RIGHT", 
		"VIM_ROTATION_MOD_HORIZ" ,
		"VIM_ROTATION_MOD_VERTI", 
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

static VIM_S32 PARAM_GetChn_AttrsFromIni(VIM_U32 u32ChnId, dictionary *ini)
{
	char keyname[128] = {0};
	
	if (ini == NULL)
		return -1;

	sprintf(keyname, "CHNID%d_CONFIG:.u32Outbit",u32ChnId);			
	ChnpAttr[u32ChnId].u32Outbit = __GetRotBitFromIniBykey(ini, keyname);
	printf("ChnpAttr[u32ChnId].u32Outbit = %d\n",ChnpAttr[u32ChnId].u32Outbit);

	sprintf(keyname, "CHNID%d_CONFIG:.u32Inbit",u32ChnId);			
	ChnpAttr[u32ChnId].u32Inbit = __GetRotBitFromIniBykey(ini, keyname);
	printf("ChnpAttr[u32ChnId].u32Inbit = %d\n",ChnpAttr[u32ChnId].u32Inbit);
	sprintf(keyname, "CHNID%d_CONFIG:.u32RotMode",u32ChnId);			
	ChnpAttr[u32ChnId].u32RotMode = __GetRotModIniBykey(ini, keyname);
	printf("ChnpAttr[u32ChnId].u32RotMode = %d\n",ChnpAttr[u32ChnId].u32RotMode);

	sprintf(keyname, "CHNID%d_CONFIG:.u32BufNum",u32ChnId);			
	ChnpAttr[u32ChnId].u32BufNum = iniparser_getint(ini, keyname, 0);
	printf("ChnpAttr[u32ChnId].u32BufNum = %d\n",ChnpAttr[u32ChnId].u32BufNum);

	sprintf(keyname, "CHNID%d_CONFIG:.u32ViIsUserBuf",u32ChnId);			
	ChnpAttr[u32ChnId].u32ViIsUserBuf = __GetBoolFromIniBykey(ini, keyname);
	printf("ChnpAttr[u32ChnId].u32ViIsUserBuf = %d\n",ChnpAttr[u32ChnId].u32ViIsUserBuf);

	sprintf(keyname, "CHNID%d_CONFIG:.u32VoIsUserBuf",u32ChnId);			
	ChnpAttr[u32ChnId].u32VoIsUserBuf = __GetBoolFromIniBykey(ini, keyname);
	printf("ChnpAttr[u32ChnId].u32VoIsUserBuf = %d\n",ChnpAttr[u32ChnId].u32VoIsUserBuf);

	sprintf(keyname, "CHNID%d_CONFIG:.u32Height",u32ChnId);			
	ChnpAttr[u32ChnId].u32Height = iniparser_getint(ini, keyname, 0);
	printf("ChnpAttr[u32ChnId].u32Height = %d\n",ChnpAttr[u32ChnId].u32Height);
	sprintf(keyname, "CHNID%d_CONFIG:.u32Width",u32ChnId);			
	ChnpAttr[u32ChnId].u32Width = iniparser_getint(ini, keyname, 0);
	printf("ChnpAttr[u32ChnId].u32Width = %d\n",ChnpAttr[u32ChnId].u32Width);
	sprintf(keyname, "CHNID%d_CONFIG:.u32Vpssbinder",u32ChnId);
	ROT_mstManInfo.u32Vpssbinder[u32ChnId] = iniparser_getint(ini, keyname, 0);
	printf("ChnpAttr[u32ChnId].u32Width = %d\n",ChnpAttr[u32ChnId].u32Width);
	sprintf(keyname, "CHNID%d_CONFIG:.u32VoShow",u32ChnId);
	ROT_mstManInfo.u32VoShow[u32ChnId] =iniparser_getint(ini, keyname, 0);




	return 0;
}

VIM_S32 ROT_PARAM_GetManCtrlInfoFromIni(char *iniName , VIM_U32 u32ChnId)
{
	//VIM_U32 u32GrpId = 0;
	VIM_U64 i = 0;
	char cmd[64] = {0};
    dictionary *ini =NULL;	
	
	ini = iniparser_load(iniName);
	if (ini == NULL)
		return -1;
	printf("----------------Get Params From: %s--------------\n", iniName);
	ROT_mstManInfo.strIniName  = (VIM_S8*)iniName;
	ROT_mstManInfo.u8DebugMask = iniparser_getint(ini, "CASE:DebugMask", 1); 
	ROT_mstManInfo.u32HeapBase = (VIM_U32)iniparser_getlonglongint(ini, "CASE:HeapBase", -1);
	ROT_mstManInfo.u32HeapSize = (VIM_U32)iniparser_getlongint(ini, "CASE:HeapSize", -1);
	ROT_mstManInfo.strSaveFileFolder = (VIM_S8*)iniparser_getstring(ini, "CASE:SaveFileFolder", NULL);
	ROT_mstManInfo.u32DynamicPlay = iniparser_getint(ini, "CASE:DynamicPlay", 0);
	// if (ROT_mstManInfo.strSaveFileFolder == NULL){
	// 	SAMPLE_PRT("please config save folders in %s ...\n", iniName);
	// 	return -1;
	// }
	ROT_mstManInfo.videosrcFile = (VIM_S8*)iniparser_getstring(ini, "CASE:videosrcFile", NULL);
	// if (ROT_mstManInfo.videosrcFile == NULL){
	// 	SAMPLE_PRT("please config save folders in %s ...\n", iniName);
	// 	return -1;
	// }
	ROT_mstManInfo.u32FpsDelayMs= iniparser_getint(ini, "CASE:u32FpsDelayMs", 0); 
	ROT_mstManInfo.u32ChnMax= iniparser_getint(ini, "CASE:u32ChnMax", 0); 
	printf("########ROT_mstManInfo.u32FpsDelayMs = %d\n",ROT_mstManInfo.u32FpsDelayMs);
	printf("########ROT_mstManInfo.u32ChnMax = %d\n",ROT_mstManInfo.u32ChnMax);
	printf("case config:\n");
	printf("\t\tDebugMask -> 0x%x\n", ROT_mstManInfo.u8DebugMask);
	printf("\t\tHeapBase -> 0x%x\n", ROT_mstManInfo.u32HeapBase);
	printf("\t\tHeapSize -> 0x%x\n", ROT_mstManInfo.u32HeapSize);
	
	sprintf(cmd, "mkdir %s -p", ROT_mstManInfo.strSaveFileFolder);
	system(cmd);

	for(i=0; i<ROT_mstManInfo.u32ChnMax; i++)
    {
		u32ChnId = i;
		if(PARAM_GetChn_AttrsFromIni(u32ChnId, ini) != 0)
			return -1;

    }
	
	return 0;
}



