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
#include "mpi_vpss.h"
#include "sample_common_vpss.h"
#include "sample_common_vpss_param.h"

dictionary *vpssini = NULL;
extern SAMPLE_MAN_INFO_S     mstManInfo;
extern VIM_VPSS_INPUT_ATTR_S chnInAttr[64];
extern VIM_CHN_CFG_ATTR_S    chnOutAttr[64][MAX_VPSS_CHN];
extern VIM_VPSS_GROUP_ATTR_S grpAttr[64];

VIM_BOOL U8ParamNum = VIM_FALSE;

VIM_BOOL __GetBoolFromIniBykey( dictionary   * ini,char *keyname)
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

VIM_U32 __GetInputTypeFromIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	VIM_U32 i = 0;
	char * inputTypeStr[] = {
		"E_INPUT_TYPE_SIF",
		"E_INPUT_TYPE_ISP",
		"E_INPUT_TYPE_MEM",
		"E_INPUT_TYPE_VIPP",
		"E_INPUT_TYPE_BUTT",
	};

	printf("keyname [%s]\n", keyname);
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

VIM_U32 __GetModTypeFromIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	VIM_U32 i = 0;
	VIM_U32 u32ModType = 0x1U;
	char * inputTypeStr[] = {
		"VIM_ID_VB",
		"VIM_ID_SYS",
		"VIM_ID_RGN",
		"VIM_ID_VDEC",
		"VIM_ID_VENC",
		"VIM_ID_VOU",
		"VIM_ID_VIU",
		"VIM_ID_AIO",
		"VIM_ID_ISP",
		"VIM_ID_ISPALG",
		"VIM_ID_JPEG",
		"VIM_ID_NPU",
		"VIM_ID_AENC",
		"VIM_ID_VPSS",
		"VIM_ID_SENSOR",
		"VIM_ID_ROT",
		"VIM_ID_BUTT",
	};

	printf("keyname [%s]\n", keyname);
	str = iniparser_getstring(ini, keyname, NULL);
	if (str == NULL)
		return 0;
	
	for (i = 0;  i < ARRAY_SIZE(inputTypeStr); i++)
	{
		if (strncmp(str, inputTypeStr[i], strlen(inputTypeStr[i])) == 0)
		{
			
			return u32ModType << i;
		}
	}
	
	return 0;
}


VIM_U32 __GetInputFormatFromIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	VIM_U32 i = 0;
	char * inputTypeStr[] = {
		"E_IMG_FORMAT_422SP",
		"E_IMG_FORMAT_420SP",
		"E_IMG_FORMAT_422P",
		"E_IMG_FORMAT_420P",
		"E_IMG_FORMAT_601P",
		"E_IMG_FORMAT_709P",
		"E_IMG_FORMAT_422I",
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

VIM_U32 __GetInputBitFromIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	VIM_U32 i = 0;
	char * inputTypeStr[] = {
		"E_IMG_BITS_8",
		"E_IMG_BITS_10",
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

VIM_U32 __GetInputYcbcrFromIniBykey( dictionary   * ini, char *keyname)
{
	const char * str = NULL;
	VIM_U32 i = 0;
	char * inputTypeStr[] = {
		"E_INPUT_YCBCR_DISABLE",
		"E_INPUT_YCBCR_GENERAL",
		"E_INPUT_YCBCR_CLIP",	
		"E_INPUT_YCBCR_TEST",
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



VIM_S32 PARAM_GetGrpIn_AttrsFromIni(VIM_U32 u32GrpId, dictionary *ini)
{
	char keyname[128] = {0};
	VIM_IMG_FORMAT_E    enFormat;
	VIM_IMG_BITS_E  	enBits;
	
	if (ini == NULL)
	{
		printf("error iniName is NULL!\n");
		return -1;
	}

	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.enFormat", u32GrpId);
	enFormat = __GetInputFormatFromIniBykey(ini, keyname);

	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.enBits",u32GrpId);
	enBits = __GetInputBitFromIniBykey(ini, keyname);
	SAMPLE_COMMON_VPSS_VpssFmtToPixFmt(enFormat, enBits, &chnInAttr[u32GrpId].enPixfmt);
	
	//sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.enYcbcr",u32GrpId);
	//chnInAttr[u32GrpId].enYcbcr = E_INPUT_YCBCR_GENERAL;
	
	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.u16Width",u32GrpId);
	chnInAttr[u32GrpId].u16Width = iniparser_getint(ini, keyname, 1280);
	//printf("PARAM_GetGrpIn_AttrsFromIni chnInAttr[%d].u16Width = %d\n",u32GrpId,chnInAttr[u32GrpId].u16Width);
	
	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.u16Height",u32GrpId);
	chnInAttr[u32GrpId].u16Height = iniparser_getint(ini, keyname, 720);
	//printf("PARAM_GetGrpIn_AttrsFromIni chnInAttr[%d].u16Height = %d\n",u32GrpId,chnInAttr[u32GrpId].u16Height);
	
	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.u16BufferNum",u32GrpId);			
	chnInAttr[u32GrpId].u16BufferNum = iniparser_getint(ini, keyname, 4);
	
	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.enCrop",u32GrpId);			
	chnInAttr[u32GrpId].enCrop = __GetBoolFromIniBykey(ini, keyname);
	
	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.u16CropLeft",u32GrpId);			
	chnInAttr[u32GrpId].u16CropLeft = iniparser_getint(ini, keyname, 10);
	
	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.u16CropTop",u32GrpId);
	chnInAttr[u32GrpId].u16CropTop = iniparser_getint(ini, keyname, 10);
	
	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.u16CropWidth",u32GrpId);
	chnInAttr[u32GrpId].u16CropWidth  = iniparser_getint(ini, keyname, 640);
	
	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.u16CropHeight",u32GrpId);
	chnInAttr[u32GrpId].u16CropHeight = iniparser_getint(ini, keyname, 480);

	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.bMirror",u32GrpId);			
	chnInAttr[u32GrpId].bMirror = __GetBoolFromIniBykey(ini, keyname);

	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.u32StrideY",u32GrpId);
	chnInAttr[u32GrpId].u32WidthStride = iniparser_getint(ini, keyname, 0);

	sprintf(keyname, "GROUP%d_VPSS_INPUT_CHN:.u32StrideUV",u32GrpId);
	chnInAttr[u32GrpId].u32HeightStride = iniparser_getint(ini, keyname, 0);

	return 0;
}


VIM_S32 PARAM_GetChnOut_AttrsFromIni(VIM_U32 u32GrpId, dictionary *ini)
{
	char keyname[128] = {0};
	VIM_U32 i = 0;
	VIM_IMG_FORMAT_E    enFormat;
	VIM_IMG_BITS_E  	enBits;

	if (ini == NULL)
	{
		printf("error iniName is NULL!\n");
		return -1;
	}

	for ( i = 0; i < MAX_VPSS_CHN; i++ )
	{
        if(! ((mstManInfo.u32ChnOutMask[u32GrpId]>>i) & 0x1))
			continue;
	
		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.enFormat",u32GrpId, i);
		enFormat = __GetInputFormatFromIniBykey(ini, keyname);
		
		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.enBits",u32GrpId, i);
		enBits = __GetInputBitFromIniBykey(ini, keyname);
		SAMPLE_COMMON_VPSS_VpssFmtToPixFmt(enFormat, enBits, &chnOutAttr[u32GrpId][i].enPixfmt);
		
		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.enYcbcr",u32GrpId, i);
		chnOutAttr[u32GrpId][i].enYcbcr = __GetInputYcbcrFromIniBykey(ini, keyname);
		
		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u16Width",u32GrpId, i);
		chnOutAttr[u32GrpId][i].u16Width = iniparser_getint(ini, keyname, 1280);
		
		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u16Height",u32GrpId, i);
		chnOutAttr[u32GrpId][i].u16Height = iniparser_getint(ini, keyname, 720);
		
		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u32Numerator",u32GrpId, i);
		chnOutAttr[u32GrpId][i].u32Numerator = iniparser_getint(ini, keyname, 30);
		
		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u32Denominator",u32GrpId, i);
		chnOutAttr[u32GrpId][i].u32Denominator = iniparser_getint(ini, keyname, 30);			
		
		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u16BufferNum",u32GrpId, i);			
		chnOutAttr[u32GrpId][i].u16BufferNum = iniparser_getint(ini, keyname, 4);

		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u32VoIsVpssBuf",u32GrpId, i);			
		chnOutAttr[u32GrpId][i].u32VoIsVpssBuf = iniparser_getint(ini, keyname, 4);

		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.enCrop",u32GrpId, i);			
		chnOutAttr[u32GrpId][i].enCrop = __GetBoolFromIniBykey(ini, keyname);

		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u16CropLeft",u32GrpId, i);			
		chnOutAttr[u32GrpId][i].u16CropLeft = iniparser_getint(ini, keyname, 0);

		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u16CropTop",u32GrpId, i);			
		chnOutAttr[u32GrpId][i].u16CropTop = iniparser_getint(ini, keyname, 0);
	
		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u16CropWidth",u32GrpId, i);			
		chnOutAttr[u32GrpId][i].u16CropWidth = iniparser_getint(ini, keyname, 0);

		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u16CropHeight",u32GrpId, i);			
		chnOutAttr[u32GrpId][i].u16CropHeight = iniparser_getint(ini, keyname, 0);

		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u32DstMod",u32GrpId, i);
		mstManInfo.u32DstMod[u32GrpId][i] = __GetModTypeFromIniBykey(ini, keyname);

		sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u32AutoFlush",u32GrpId, i);
		chnOutAttr[u32GrpId][i].u32AutoFlush = iniparser_getint(ini, keyname, 0);

		printf("mstManInfo get u32GrpId:%d chn:%d u32DstMod:0x%x u32AutoFlush:%d u32ChnOutMask:0x%x.... \n",u32GrpId, i, mstManInfo.u32DstMod[u32GrpId][i],chnOutAttr[u32GrpId][i].u32AutoFlush, mstManInfo.u32ChnOutMask[u32GrpId]);
		if (i == VPSS_CHN1)
		{
			sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.enCframe",u32GrpId, i);			
			chnOutAttr[u32GrpId][i].enCframe = __GetBoolFromIniBykey(ini, keyname);

			sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.enLoss",u32GrpId, i);			
			chnOutAttr[u32GrpId][i].enLoss = __GetBoolFromIniBykey(ini, keyname);
			
			sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u8BitDepth",u32GrpId, i);			
			chnOutAttr[u32GrpId][i].u8BitDepth = iniparser_getint(ini, keyname, 64);
			
			sprintf(keyname, "GROUP%d_VPSS_OUTPUT_CHN%d:.u8BitDepthu",u32GrpId, i);			
			chnOutAttr[u32GrpId][i].u8BitDepthu = iniparser_getint(ini, keyname, 64);
		}
		else
		{
			chnOutAttr[u32GrpId][i].enCframe = VIM_FALSE;
		}
	}

	return 0;
}


VIM_S32 PARAM_GetGroup_AttrsFromIni(VIM_U32 u32GrpId, dictionary *ini, VIM_U32 Mask)
{
	char keyname[128] = {0};
	
	if (ini == NULL)
	{
		printf("error iniName is NULL!\n");
		return -1;
	}
		
	sprintf(keyname, "GROUP%d_CONFIG:.u32GrpStatus",u32GrpId);			
	grpAttr[u32GrpId].u32GrpStatus = iniparser_getint(ini, keyname, 0);

	sprintf(keyname, "GROUP%d_CONFIG:.enInputType",u32GrpId);			
	grpAttr[u32GrpId].enInputType = iniparser_getint(ini, keyname, 0);

	sprintf(keyname, "GROUP%d_CONFIG:.u32GrpDataSrc",u32GrpId);			
	grpAttr[u32GrpId].u32GrpDataSrc = iniparser_getint(ini, keyname, 0);

	sprintf(keyname, "GROUP%d_CONFIG:.u32ViIsVpssBuf",u32GrpId);			
	grpAttr[u32GrpId].u32ViIsVpssBuf = iniparser_getint(ini, keyname, 0);

	sprintf(keyname, "GROUP%d_CONFIG:.u32SrcMod",u32GrpId);
	mstManInfo.u32SrcMod = __GetModTypeFromIniBykey(ini, keyname);
	printf(" u32SrcMod:0x%x.... \n",mstManInfo.u32SrcMod);
	if(Mask >= 0xff)
	{
		sprintf(keyname, "GROUP%d_CONFIG:.u32ChnOutMask",u32GrpId);
		mstManInfo.u32ChnOutMask[u32GrpId] = iniparser_getint(ini, keyname, 0);
	}
	else
	{
		sprintf(keyname, "GROUP%d_CONFIG:.u32ChnOutMask",u32GrpId);
		mstManInfo.u32ChnOutMask[u32GrpId] = Mask;
	}

	grpAttr[u32GrpId].u32GrpId = u32GrpId;

	return 0;
}



VIM_S32 PARAM_GetManCtrlInfoFromIni(char *iniName , VIM_U32 Mask, VIM_U32 u32GrpId)
{
	VIM_S32 s32StartInputFrameNo = 0;
	VIM_U64 i = 0;
	VIM_S32 s32Ret = 0;
	char cmd[64] = {0};
    dictionary *ini =NULL;

	if(vpssini == NULL)
	{
		vpssini = iniparser_load(iniName);
		if (vpssini == NULL)
		{
			printf("error iniName is NULL!\n");
			return -1;
		}
	}

	ini = vpssini;
		
	printf("----------------Get Params From: %s--------------\n", iniName);
	mstManInfo.strIniName  = iniName;
	mstManInfo.u8DebugMask = iniparser_getint(ini, "CASE:DebugMask", 1); 
	mstManInfo.u32OsdOpen = (VIM_U32)iniparser_getlonglongint(ini, "CASE:OsdOpen", -1);
	mstManInfo.u32OsdColor = (VIM_U32)iniparser_getlongint(ini, "CASE:OsdColor", -1);
	mstManInfo.bSaveFile   = iniparser_getint(ini, "CASE:bSaveFile", 0);
	mstManInfo.bSaveSingleFile   = iniparser_getint(ini, "CASE:SaveSingleFile", -1);
	mstManInfo.strSaveFileFolder = iniparser_getstring(ini, "CASE:SaveFileFolder", NULL);
	mstManInfo.u32DynamicPlay = iniparser_getint(ini, "CASE:DynamicPlay", 0);
	// if (mstManInfo.strSaveFileFolder == NULL){
	// 	SAMPLE_PRT("please config save folders in %s ...\n", iniName);
	// 	return -1;
	// }
	mstManInfo.videosrcFile = iniparser_getstring(ini, "CASE:videosrcFile", NULL);
	// if (mstManInfo.videosrcFile == NULL){
	// 	SAMPLE_PRT("please config save folders in %s ...\n", iniName);
	// 	return -1;
	// }

	s32StartInputFrameNo   = iniparser_getint(ini, "CASE:StartInputFrameNo", -1);
	for(i = 0; i < ARRAY_SIZE(mstManInfo.s32StartInputFrameNo); i++ )
	{
		mstManInfo.s32StartInputFrameNo[i] = s32StartInputFrameNo;
	}
	
	mstManInfo.bEnableInputFrameNo = iniparser_getint(ini, "CASE:bEnableInputFrameNo", 0);
	mstManInfo.u32FpsDelayMs= iniparser_getint(ini, "CASE:u32FpsDelayMs", 0); 
	mstManInfo.u32GrpMask   = iniparser_getint(ini, "CASE:u32GrpMask", 0);

	printf("case config:\n");
	printf("\t\tDebugMask -> 0x%x\n", mstManInfo.u8DebugMask);
	printf("\t\tOsdOpen -> 0x%x\n", mstManInfo.u32OsdOpen);
	printf("\t\tOsdColor -> 0x%x\n", mstManInfo.u32OsdColor);
	printf("\t\tSaveFile -> 0x%x\n", mstManInfo.bSaveFile);
	printf("\t\tGrpMask -> 0x%x\n", mstManInfo.u32GrpMask);
	sprintf(cmd, "mkdir %s -p", mstManInfo.strSaveFileFolder);
	system(cmd);

	if (u32GrpId >= 0xff)
	{	
		for(i=0; i<16; i++)
	    {
            if(0 == ((mstManInfo.u32GrpMask>>i) & 0x1))
				continue;
			if(PARAM_GetGroup_AttrsFromIni(i, ini, Mask) != 0) {
				s32Ret = -1;
				goto end;
			}

			if(PARAM_GetGrpIn_AttrsFromIni(i, ini) != 0) {
				s32Ret = -1;
				goto end;
			}

			if(PARAM_GetChnOut_AttrsFromIni(i, ini) != 0) {
				s32Ret = -1;
				goto end;
			}
	    }
	}
	else
	{
		if(PARAM_GetGroup_AttrsFromIni(u32GrpId, ini, Mask) != 0) {
				s32Ret = -1;
				goto end;
		}

		if(PARAM_GetGrpIn_AttrsFromIni(u32GrpId, ini) != 0) {
				s32Ret = -1;
				goto end;
		}

		if(PARAM_GetChnOut_AttrsFromIni(u32GrpId, ini) != 0) {
				s32Ret = -1;
				goto end;
		}
	}
end:

	return s32Ret;
}



