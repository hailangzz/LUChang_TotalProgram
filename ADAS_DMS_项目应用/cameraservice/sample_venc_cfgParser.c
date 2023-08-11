///<------------------------------------------------------------------------------
///< File: sample_venc_cfgParser.c
///<
///< Copyright (c) 2020,VIMICRO.  All rights reserved.
///<------------------------------------------------------------------------------
#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#include "sample_venc_cfgParser.h"

#define SEEK_SET 0
#define MAX_CFG (178)
typedef void *osal_file_t;

#ifndef SAMPLE_PRT
#define SAMPLE_PRT(fmt...)                             \
	do                                                 \
	{                                                  \
	    printf("[%s] - %d: ", __func__, __LINE__); \
	    printf(fmt);                                   \
	} while (0)
#endif

const CfgInfo waveCfgInfo[] = {
    ///< name  default
    {"u8MaxChnNum", 0}, ///< 0
    {"u32MaxPicWidth", 0},
    {"u32MaxPicHeight", 0},
    {"u32MaxPixelMode", 0},
    {"u32MaxTargetFrmRate", 0},
    {"VencFwPath", 1},
    {"VeChn", 0},
    {"ViChn", 0},
    {"venc_bind", 0},
    {"enType", 0},
    {"u32BufSize", 0}, ///< 10
    {"u32Profile", 0},
    {"bByFrame", 0},
    {"u32PicWidth", 0},
    {"u32PicHeight", 0},
    {"enRcMode", 0},
    {"enPixelFormat", 1},
    {"u32PixelMode", 0},
    {"camBufCnt", 0},
    {"u32Gop", 0},
    {"u32StatTime", 0}, ///< 20
    {"u32SrcFrmRate", 0},
    {"fr32TargetFrmRate", 0},
    {"u32BitRate", 0},
    {"u32FluctuateLevel", 0},
    {"u32MaxQp", 0},
    {"u32MinQp", 0},
    {"specialheader", 0},
    {"staicRefPerid", 0},
    {"u32IQp", 0},
    {"u32PQp", 0}, ///< 30
    {"u32BQp", 0},
    {"bEnableSvc", 0},
    {"u32SvcQp", 0},
    {"bEnableTsvc", 0},
    {"EncBitrate", 0},
    {"u32Index", 0},
    {"bEnable", 0},
    {"bAbsQp", 0},
    {"s32Qp", 0},
    {"s32Quality", 0}, ///< 40
    {"bBKEnable", 0},
    {"s32X", 0},
    {"s32Y", 0},
    {"u32Width", 0},
    {"u32Height", 0},
};

/*
 * ENCODE PARAMETER PARSE FUNCSIONS
 */
int GetValue(osal_file_t fp, char *para, char *value)
{
    char lineStr[256];
    char paraStr[256];

    fseek(fp, 0, 0);

    while (1)
    {
        if (fgets(lineStr, 256, fp) == NULL)
            return 0;
        sscanf(lineStr, "%s %s", paraStr, value);
        if (paraStr[0] != ';')
        {
            if (strcmp(para, paraStr) == 0)
                return 1;
        }
    }
}

///< Parse "string number number ..." at most "num" numbers
///< e.g. SKIP_PIC_NUM 1 3 4 5
int GetValues(osal_file_t fp, char *para, int *values, int num)
{
    char line[1024];

    fseek(fp, 0, 0);

    while (1)
    {
        int i;
        char *str;

        if (fgets(line, sizeof(line) - 1, fp) == NULL)
            return 0;

        ///< empty line
        if (NULL == (str = strtok(line, " ")))
            continue;

        if (strcmp(str, para) != 0)
            continue;

        for (i = 0; i < num; i++)
        {
            if (NULL == (str = strtok(NULL, " ")))
                return 1;
            if (!isdigit((int)str[0]))
                return 1;
            values[i] = atoi(str);
        }
        return 1;
    }
}

int WAVE_GetStringValue(osal_file_t fp, char *para, char *value, unsigned int offset)
{
    int pos = 0;
    char *token = NULL;
    char lineStr[256] = {0,};
    char valueStr[256] = {0,};

    fseek(fp, offset, 0);

    while (1)
    {
        if (fgets(lineStr, 256, fp) == NULL)
        {
            return 0; ///< not exist para in cfg file
        }

        if ((lineStr[0] == '#') || (lineStr[0] == ';') || (lineStr[0] == ':'))
        {
            ///< check comment
            continue;
        }

        token = strtok(lineStr, ": "); ///< parameter name is separated by ' ' or ':'
        if (token != NULL)
        {
            if (strcasecmp(para, token) == 0)
            {
                ///< check parameter name
                token = strtok(NULL, ":\r\n");
                if (token == NULL)
                    return -1;
                memcpy(valueStr, token, strlen(token));
                while (valueStr[pos] == ' ')
                {
                    ///< check space
                    pos++;
                }
                if (valueStr[pos] == 0)
                    return -1; ///< no value

                strcpy(value, &valueStr[pos]);

				return 1;
            }
            else
            {
                continue;
            }
        }
        else
        {
            continue;
        }
    }
}

int WAVE_GetValue(osal_file_t fp, char *cfgName, int *value, unsigned int offset)
{
    int i = 0;
    int iValue = 0;
    int ret;
    char sValue[256] = {0,};

    if (i == MAX_CFG)
    {
        printf("CFG param error : %s\n", cfgName);

        return 0;
    }

    ret = WAVE_GetStringValue(fp, cfgName, sValue, offset);
    if (ret == 1)
    {
        iValue = atoi(sValue);
        *value = iValue;
    }
    else if (ret == -1)
    {
        printf("warn %s=%s iValue=%d\n", __func__, cfgName, iValue);

        return 0;
    }
    else
    {
        printf("warn  %s=%s iValue=%d\n", __func__, cfgName, iValue);

        return 0;
    }

    return ret;
}

int WAVE_GetValue_New(osal_file_t fp, char *cfgName, int *value, unsigned int offset, unsigned int offset_end)
{
    int iValue = 0;
    int ret = 0;
    char sValue[256] = {0,};
    int value_offset_end = 0;

    ret = WAVE_GetStringValue(fp, cfgName, sValue, offset);
    if (ret == 1)
    {
        iValue = atoi(sValue);
        *value = iValue;
        ///< printf(" WAVE_GetValue cfgName=%s iValue=%d\n",cfgName,iValue);
        GetValueCfgOffset(fp, cfgName, iValue, offset, &value_offset_end);

        if ((unsigned int)value_offset_end >= offset_end)
        {
            *value = 0;
        }
    }
    else if (ret == -1)
    {
        printf(" warn WAVE_GetValue=%s iValue=%d\n", cfgName, iValue);

        return 0;
    }
    else
    {
        printf(" warn  WAVE_GetValue=%s iValue=%d\n", cfgName, iValue);

        return 0;
    }

    return ret;
}

void printParsePara(SAMPLE_ENC_CFG *pEncCfg)
{
    SAMPLE_PRT(" ennable_nodebug = %d\n", pEncCfg->ennable_nodebug);
    if (pEncCfg->ennable_nodebug)
    {
        return;
    }

    SAMPLE_PRT(" u8MaxChnNum = %d\n", pEncCfg->u8MaxChnNum);
    SAMPLE_PRT(" u8FlagCodeAndDecorder = %d\n", pEncCfg->u8FlagCodeAndDecorder);
    SAMPLE_PRT(" u32MaxPicWidth = %d\n", pEncCfg->u32MaxPicWidth);
    SAMPLE_PRT(" u32MaxPicHeight = %d\n", pEncCfg->u32MaxPicHeight);
    SAMPLE_PRT(" u32MaxPixelMode = %d\n", pEncCfg->u32MaxPixelMode);
    SAMPLE_PRT(" u32MaxTargetFrmRate = %d\n", pEncCfg->u32MaxTargetFrmRate);
    SAMPLE_PRT(" VencFwPath = %s\n", pEncCfg->VencFwPath);
    SAMPLE_PRT(" VeChn = %d\n", pEncCfg->VeChn);
    SAMPLE_PRT(" BindChn = %d\n", pEncCfg->BindChn);
    SAMPLE_PRT(" BindChnBL = %d\n", pEncCfg->BindChnBL);
    SAMPLE_PRT(" flag_2vdsp = %d\n", pEncCfg->flag_2vdsp);
    SAMPLE_PRT(" venc_bind = %d\n", pEncCfg->venc_bind);
    SAMPLE_PRT(" enType = %d\n", pEncCfg->enType);
    SAMPLE_PRT(" u32BufSize = %d\n", pEncCfg->u32BufSize);
    SAMPLE_PRT(" u32Profile = %d\n", pEncCfg->u32Profile);
    SAMPLE_PRT(" bByFrame = %d\n", pEncCfg->bByFrame);
    SAMPLE_PRT(" u32PicWidth = %d\n", pEncCfg->u32PicWidth);
    SAMPLE_PRT(" u32PicHeight = %d\n", pEncCfg->u32PicHeight);
    SAMPLE_PRT(" enRcMode = %d\n", pEncCfg->enRcMode);
    SAMPLE_PRT(" enPixelFormat = %s\n", pEncCfg->enPixelFormat);
    SAMPLE_PRT(" u32PixelMode = %d\n", pEncCfg->u32PixelMode);

    SAMPLE_PRT(" src_enFormat = %d\n", pEncCfg->src_enFormat);
    SAMPLE_PRT(" dst_enFormat = %d\n", pEncCfg->dst_enFormat);

    SAMPLE_PRT(" u32Gop = %d\n", pEncCfg->u32Gop);
    SAMPLE_PRT(" u32StatTime = %d\n", pEncCfg->u32StatTime);
    SAMPLE_PRT(" fr32TargetFrmRate = %d\n", pEncCfg->fr32TargetFrmRate);
    SAMPLE_PRT(" u32BitRate = %d\n", pEncCfg->u32BitRate);
    SAMPLE_PRT(" u32FluctuateLevel = %d\n", pEncCfg->u32FluctuateLevel);

    SAMPLE_PRT(" u32MaxQp = %d\n", pEncCfg->u32MaxQp);
    SAMPLE_PRT(" u32MinQp = %d\n", pEncCfg->u32MinQp);

    SAMPLE_PRT(" specialheader = %d\n", pEncCfg->specialheader);
    SAMPLE_PRT(" staicRefPerid = %d\n", pEncCfg->staicRefPerid);
    SAMPLE_PRT(" u32IQp = %d\n", pEncCfg->u32IQp);
    SAMPLE_PRT(" u32PQp = %d\n", pEncCfg->u32PQp);
    SAMPLE_PRT(" u32BQp = %d\n", pEncCfg->u32BQp);
    SAMPLE_PRT(" camBufCnt = %d\n", pEncCfg->camBufCnt);

    SAMPLE_PRT(" bcframeDataType = %d\n", pEncCfg->bcframeDataType);
    SAMPLE_PRT(" bcframe50Enable = %d\n", pEncCfg->bcframe50Enable);
    SAMPLE_PRT(" bcframe50LosslessEnable = %d\n", pEncCfg->bcframe50LosslessEnable);
    SAMPLE_PRT(" cframe50Tx16Y = %d\n", pEncCfg->cframe50Tx16Y);
    SAMPLE_PRT(" cframe50Tx16C = %d\n", pEncCfg->cframe50Tx16C);

    SAMPLE_PRT(" bEnableSvc = %d\n", pEncCfg->bEnableSvc);
    SAMPLE_PRT(" u8SvcMode = %d\n", pEncCfg->u8SvcMode);
    SAMPLE_PRT(" u8SvcRation = %d\n", pEncCfg->u8SvcRation);

    SAMPLE_PRT(" bEnableTsvc = %d\n", pEncCfg->bEnableTsvc);
    SAMPLE_PRT(" u32Index = %d\n", pEncCfg->u32Index);
    SAMPLE_PRT(" bEnable = %d\n", pEncCfg->bEnable);
    SAMPLE_PRT(" bAbsQp = %d\n", pEncCfg->bAbsQp);
    SAMPLE_PRT(" s32Qp = %d\n", pEncCfg->s32Qp);
    SAMPLE_PRT(" s32AvgOp = %d\n", pEncCfg->s32AvgOp);
    SAMPLE_PRT(" s32Quality = %d\n", pEncCfg->s32Quality);
    SAMPLE_PRT(" bBKEnable = %d\n", pEncCfg->bBKEnable);
    SAMPLE_PRT(" s32X = %d\n", pEncCfg->s32X);
    SAMPLE_PRT(" s32Y = %d\n", pEncCfg->s32Y);
    SAMPLE_PRT(" u32Width = %d\n", pEncCfg->u32Width);
    SAMPLE_PRT(" u32Height = %d\n", pEncCfg->u32Height);
    SAMPLE_PRT(" runtime_sec = %d\n", pEncCfg->runtime_sec);
    SAMPLE_PRT(" enable_nosavefile = %d\n", pEncCfg->enable_nosavefile);
    SAMPLE_PRT(" streamname = %s\n", pEncCfg->streamname);
    SAMPLE_PRT(" sensor_type = %d\n", pEncCfg->sensor_type);
    SAMPLE_PRT(" dynamic_config_svc = %d\n", pEncCfg->dynamic_config_svc);
    SAMPLE_PRT(" dynamic_config_chn_flag = %d\n", pEncCfg->dynamic_config_chn_flag);
    SAMPLE_PRT(" dynamic_config_flag = %d\n", pEncCfg->dynamic_config_flag);
    SAMPLE_PRT(" dynamic_config_num = %d\n", pEncCfg->dynamic_config_num);
    SAMPLE_PRT(" dymic_config_time_msleep = %d\n", pEncCfg->dymic_config_time_msleep);
    SAMPLE_PRT(" enable_flag = %d\n", pEncCfg->enable_flag);
    SAMPLE_PRT(" chn_enable_flag = %d\n", pEncCfg->chn_enable_flag);
    SAMPLE_PRT(" local_encode_flag = %d\n", pEncCfg->local_encode_flag);
    SAMPLE_PRT(" souce_filename = %s\n", pEncCfg->souce_filename);
    SAMPLE_PRT(" souce_filenameBL = %s\n", pEncCfg->souce_filenameBL);
    SAMPLE_PRT(" souce_framenum = %d\n", pEncCfg->souce_framenum);

    SAMPLE_PRT(" bEnableLongTerm = %d\n", pEncCfg->bEnableLongTerm);
    SAMPLE_PRT(" u32AsLongtermPeriod = %d\n", pEncCfg->u32AsLongtermPeriod);
    SAMPLE_PRT(" u32refLongtermPeriod = %d\n", pEncCfg->u32refLongtermPeriod);
    SAMPLE_PRT(" EnSAO = %d\n", pEncCfg->EnSAO);
    SAMPLE_PRT(" bEnableRotation = %d\n", pEncCfg->bEnableRotation);
    SAMPLE_PRT(" u32rotAngle = %d\n", pEncCfg->u32rotAngle);
    SAMPLE_PRT(" enmirDir = %d\n", pEncCfg->enmirDir);
    SAMPLE_PRT(" RoiFlag = %d\n", pEncCfg->RoiFlag);
    SAMPLE_PRT(" EnRoi = %d\n", pEncCfg->EnRoi);

    SAMPLE_PRT(" test_async_mode = %d\n", pEncCfg->test_async_mode);
    SAMPLE_PRT(" roi_file_name = %s\n", pEncCfg->roi_file_name);

    SAMPLE_PRT(" filesize_num = %d\n", pEncCfg->filesize_num);
    SAMPLE_PRT(" reserved[0] = %d\n", pEncCfg->reserved[0]);
    SAMPLE_PRT(" reserved[1] = %d\n", pEncCfg->reserved[1]);
    SAMPLE_PRT(" reserved[2] = %d\n", pEncCfg->reserved[2]);
    SAMPLE_PRT(" reserved[3] = %d\n", pEncCfg->reserved[3]);
    SAMPLE_PRT(" reserved_str = %s\n", pEncCfg->reserved_str);

    return;
}

int parseEncCfgFile(SAMPLE_ENC_CFG *pEncCfg, char *FileName, unsigned int offset, unsigned int Offset_end)
{
    FILE *fp = NULL;
    char sValue[1024] = {0};
    int iValue = 0;
    int ret = 0;
    int values_offset_end = 0;

    fp = fopen(FileName, "r");
    if (fp == NULL)
    {
        SAMPLE_PRT("file open err : %s, errno(%d)\n", FileName, errno);
        return ret;
    }

    if (WAVE_GetValue_New(fp, "u8MaxChnNum", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u8MaxChnNum = iValue;
    }

    if (WAVE_GetValue_New(fp, "u8FlagCodeAndDecorder", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->u8FlagCodeAndDecorder = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32MaxPicWidth", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32MaxPicWidth = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32MaxPicHeight", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32MaxPicHeight = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32MaxPixelMode", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32MaxPixelMode = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32MaxTargetFrmRate", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32MaxTargetFrmRate = iValue;
    }

    memset(sValue, 0x0, sizeof(sValue));
    if (WAVE_GetStringValue(fp, "VencFwPath", sValue, offset) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        strcpy(pEncCfg->VencFwPath, sValue);
    }

    if (WAVE_GetValue_New(fp, "VeChn", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->VeChn = iValue;
    }

    if (WAVE_GetValue_New(fp, "BindChn", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->BindChn = iValue;
    }

    if (WAVE_GetValue_New(fp, "BindChnBL", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->BindChnBL = iValue;
    }

    if (WAVE_GetValue_New(fp, "flag_2vdsp", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->flag_2vdsp = iValue;
    }

    if (WAVE_GetValue_New(fp, "venc_bind", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->venc_bind = iValue;
    }

    if (WAVE_GetValue_New(fp, "enType", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->enType = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32BufSize", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32BufSize = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32Profile", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32Profile = iValue;
    }

    if (WAVE_GetValue_New(fp, "bByFrame", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->bByFrame = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32PicWidth", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32PicWidth = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32PicHeight", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32PicHeight = iValue;
    }

    if (WAVE_GetValue_New(fp, "enRcMode", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->enRcMode = iValue;
    }

    if (WAVE_GetValue_New(fp, "enGopMode", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->enGopMode = iValue;
    }

    if (WAVE_GetValue_New(fp, "src_enFormat", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->src_enFormat = iValue;
    }

    if (WAVE_GetValue_New(fp, "dst_enFormat", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->dst_enFormat = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32Gop", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32Gop = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32StatTime", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32StatTime = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32SrcFrmRate", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32SrcFrmRate = iValue;
    }

    if (WAVE_GetValue_New(fp, "fr32TargetFrmRate", &iValue, offset, Offset_end) == 0)
    {
        ///<		goto __end_parse;
    }
    else
    {
        pEncCfg->fr32TargetFrmRate = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32BitRate", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32BitRate = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32FluctuateLevel", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32FluctuateLevel = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32MaxQp", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32MaxQp = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32MinQp", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32MinQp = iValue;
    }

    if (WAVE_GetValue_New(fp, "specialheader", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->specialheader = iValue;
    }

    if (WAVE_GetValue_New(fp, "staicRefPerid", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->staicRefPerid = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32IQp", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32IQp = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32PQp", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32PQp = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32BQp", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32BQp = iValue;
    }

    if (WAVE_GetValue_New(fp, "camBufCnt", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->camBufCnt = iValue;
    }

    if (WAVE_GetValue_New(fp, "bcframeDataType", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->bcframeDataType = iValue;
    }

    if (WAVE_GetValue_New(fp, "bcframe50Enable", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->bcframe50Enable = iValue;
    }

    if (WAVE_GetValue_New(fp, "bcframe50LosslessEnable", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->bcframe50LosslessEnable = iValue;
    }

    if (WAVE_GetValue_New(fp, "cframe50Tx16Y", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->cframe50Tx16Y = iValue;
    }

    if (WAVE_GetValue_New(fp, "cframe50Tx16C", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->cframe50Tx16C = iValue;
    }

    if (WAVE_GetValue_New(fp, "bEnableSvc", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->bEnableSvc = iValue;
    }

    if (WAVE_GetValue_New(fp, "bEnableTsvc", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->bEnableTsvc = iValue;
    }

    if (WAVE_GetValue_New(fp, "u8SvcMode", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u8SvcMode = iValue;
    }

    if (WAVE_GetValue_New(fp, "u8SvcRation", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u8SvcRation = iValue;
    }

    if (WAVE_GetValue_New(fp, "bEnableLongTerm", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->bEnableLongTerm = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32AsLongtermPeriod", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->u32AsLongtermPeriod = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32refLongtermPeriod", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->u32refLongtermPeriod = iValue;
    }

    if (WAVE_GetValue_New(fp, "EnSAO", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->EnSAO = iValue;
    }

    if (WAVE_GetValue_New(fp, "RoiFlag", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->RoiFlag = iValue;
    }

    if (WAVE_GetValue_New(fp, "EnRoi", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->EnRoi = iValue;
    }

    if (WAVE_GetValue_New(fp, "bEnableRotation", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->bEnableRotation = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32rotAngle", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->u32rotAngle = iValue;
    }

    if (WAVE_GetValue_New(fp, "enmirDir", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->enmirDir = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32Index", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->u32Index = iValue;
    }

    if (WAVE_GetValue_New(fp, "bEnable", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->bEnable = iValue;
    }

    if (WAVE_GetValue_New(fp, "bAbsQp", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->bAbsQp = iValue;
    }

    if (WAVE_GetValue_New(fp, "s32Qp", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->s32Qp = iValue;
    }

    if (WAVE_GetValue_New(fp, "s32AvgOp", &iValue, offset, Offset_end) == 0)
    {
        ///<		  goto __end_parse;
    }
    else
    {
        pEncCfg->s32AvgOp = iValue;
    }

    if (WAVE_GetValue_New(fp, "s32Quality", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->s32Quality = iValue;
    }

    if (WAVE_GetValue_New(fp, "bBKEnable", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->bBKEnable = iValue;
    }

    if (WAVE_GetValue_New(fp, "s32X", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->s32X = iValue;
    }

    if (WAVE_GetValue_New(fp, "s32Y", &iValue, offset, Offset_end) == 0)
    {
        ///<        goto __end_parse;
    }
    else
    {
        pEncCfg->s32Y = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32Width", &iValue, offset, Offset_end) == 0)
    {
        SAMPLE_PRT("parase the enable_flag  error iValue = %d\n", iValue);
        pEncCfg->u32Width = 0;
    }
    else
    {
        pEncCfg->u32Width = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32Height", &iValue, offset, Offset_end) == 0)
    {
        SAMPLE_PRT("parase the enable_flag  error iValue = %d\n", iValue);
        pEncCfg->u32Height = 0;
    }
    else
    {
        pEncCfg->u32Height = iValue;
    }

    if (WAVE_GetValue_New(fp, "runtime_sec", &iValue, offset, Offset_end) == 0)
    {
        SAMPLE_PRT("parase the enable_flag  error iValue = %d\n", iValue);
        pEncCfg->runtime_sec = 0;
    }
    else
    {
        pEncCfg->runtime_sec = iValue;
    }

    if (WAVE_GetValue_New(fp, "enable_nosavefile", &iValue, offset, Offset_end) == 0)
    {
        SAMPLE_PRT("parase enable_nosavefile  error iValue = %d\n", iValue);
        pEncCfg->enable_nosavefile = 0;
        ///< goto __end_parse;
    }
    else
    {
        pEncCfg->enable_nosavefile = iValue;
    }

    memset(sValue, 0x0, sizeof(sValue));
    if (WAVE_GetStringValue(fp, "streamname", (char *)&sValue, offset) == 0)
    {
        SAMPLE_PRT("parase the streamname  error iValue = %s\n", (char *)iValue);
        memset(pEncCfg->streamname, 0x0, MAX_FILE_PATH);
        ///< goto __end_parse;
    }
    else
    {
        strcpy(pEncCfg->streamname, sValue);
        ///< pEncCfg->enPixelFormat = sValue;
    }

    if (WAVE_GetValue_New(fp, "sensor_type", &iValue, offset, Offset_end) == 0)
    {
        SAMPLE_PRT("parase the sensor_type  error iValue = %d\n", iValue);
        pEncCfg->sensor_type = 0;
        ///< goto __end_parse;
    }
    else
    {
        pEncCfg->sensor_type = iValue;
    }

    if (WAVE_GetValue_New(fp, "dynamic_config_chn_flag", &iValue, offset, Offset_end) == 0)
    {
        ///< SAMPLE_PRT( "parase the dynamic_config_chn_flag  error iValue = %d\n",iValue);
        pEncCfg->dynamic_config_chn_flag = 0;
    }
    else
    {
        pEncCfg->dynamic_config_chn_flag = iValue;
    }

    if (WAVE_GetValue_New(fp, "dymic_config_time_msleep", &iValue, offset, Offset_end) == 0)
    {
        ///<		SAMPLE_PRT( "parase the dymic_config_time_msleep  error iValue = %d\n",iValue);
        pEncCfg->dymic_config_time_msleep = 0;
    }
    else
    {
        pEncCfg->dymic_config_time_msleep = iValue;
    }

    if (WAVE_GetValue_New(fp, "enable_flag", &iValue, offset, Offset_end) == 0)
    {
        ///<		SAMPLE_PRT( "parase the enable_flag  error iValue = %d\n",iValue);
        pEncCfg->enable_flag = 0;
    }
    else
    {
        pEncCfg->enable_flag = iValue;
    }

    if (WAVE_GetValue_New(fp, "ennable_nodebug", &iValue, offset, Offset_end) == 0)
    {
        ///<		SAMPLE_PRT( "parase the enable_flag  error iValue = %d\n",iValue);
        pEncCfg->ennable_nodebug = 0;
    }
    else
    {
        pEncCfg->ennable_nodebug = iValue;
    }

    if (WAVE_GetValue_New(fp, "test_async_mode", &iValue, offset, Offset_end) == 0)
    {
        ///<		SAMPLE_PRT( "parase the enable_flag  error iValue = %d\n",iValue);
        pEncCfg->test_async_mode = 0;
    }
    else
    {
        pEncCfg->test_async_mode = iValue;
    }

    if (WAVE_GetValue_New(fp, "local_encode_flag", &iValue, offset, Offset_end) == 0)
    {
        ///<		SAMPLE_PRT( "parase the local_encode_flag  error iValue = %d\n",iValue);
        pEncCfg->local_encode_flag = 0;
    }
    else
    {
        pEncCfg->local_encode_flag = iValue;
    }

    if (WAVE_GetValue_New(fp, "souce_framenum", &iValue, offset, Offset_end) == 0)
    {
        SAMPLE_PRT("parase the souce_framenum  error iValue = %d\n", iValue);
        pEncCfg->souce_framenum = 0;
    }
    else
    {
        pEncCfg->souce_framenum = iValue;
    }

    if (WAVE_GetValue_New(fp, "filesize_num", &iValue, offset, Offset_end) == 0)
    {
        ///< SAMPLE_PRT( "parase the filesize_num  error iValue = %d\n",iValue);
        pEncCfg->filesize_num = 0;
    }
    else
    {
        pEncCfg->filesize_num = iValue;
    }

    if (WAVE_GetValue_New(fp, "reserved[0]", &iValue, offset, Offset_end) == 0)
    {
        ///< SAMPLE_PRT( "parase the reserved[0]  error iValue = %d\n",iValue);
        pEncCfg->reserved[0] = 0;
    }
    else
    {
        pEncCfg->reserved[0] = iValue;
    }

    if (WAVE_GetValue_New(fp, "reserved[1]", &iValue, offset, Offset_end) == 0)
    {
        ///< SAMPLE_PRT( "parase the reserved[1]  error iValue = %d\n",iValue);
        pEncCfg->reserved[1] = 0;
    }
    else
    {
        pEncCfg->reserved[1] = iValue;
    }

    if (WAVE_GetValue_New(fp, "reserved[2]", &iValue, offset, Offset_end) == 0)
    {
        ///< SAMPLE_PRT( "parase the reserved[2]  error iValue = %d\n",iValue);
        pEncCfg->reserved[2] = 0;
    }
    else
    {
        pEncCfg->reserved[2] = iValue;
    }

    if (WAVE_GetValue_New(fp, "reserved[3]", &iValue, offset, Offset_end) == 0)
    {
        ///< SAMPLE_PRT( "parase the reserved[3]  error iValue = %d\n",iValue);
        pEncCfg->reserved[3] = 0;
    }
    else
    {
        pEncCfg->reserved[3] = iValue;
    }

    memset(sValue, 0x0, sizeof(sValue));
    if (WAVE_GetStringValue(fp, "souce_filename", (char *)&sValue, offset) == 0)
    {
        SAMPLE_PRT("parase the souce_filename  error sValue = %s\n", sValue);
        memset(pEncCfg->souce_filename, 0x0, MAX_FILE_PATH);
        ///< goto __end_parse;
    }
    else
    {
        strcpy(pEncCfg->souce_filename, sValue);
        ///< pEncCfg->enPixelFormat = sValue;
    }

    memset(sValue, 0x0, sizeof(sValue));
    if (WAVE_GetStringValue(fp, "souce_filenameBL", (char *)&sValue, offset) == 0)
    {
        SAMPLE_PRT("parase the souce_filenameBL  error sValue = %s\n", sValue);
        memset(pEncCfg->souce_filenameBL, 0x0, MAX_FILE_PATH);
        ///< goto __end_parse;
    }
    else
    {
        strcpy(pEncCfg->souce_filenameBL, sValue);
        ///< pEncCfg->enPixelFormat = sValue;
    }

    memset(sValue, 0x0, sizeof(sValue));
    if (WAVE_GetStringValue(fp, "RoiFile", (char *)&sValue, offset) == 0)
    {
        SAMPLE_PRT("parase the roi_file_name  error sValue = %s\n", sValue);
        memset(pEncCfg->roi_file_name, 0x0, MAX_FILE_PATH);
        ///< goto __end_parse;
    }
    else
    {
        strcpy(pEncCfg->roi_file_name, sValue);
        GetStringCfgOffset(fp, "RoiFile", sValue, offset, &values_offset_end);
        SAMPLE_PRT("values_offset_end=0x%x Offset_end=0x%x\n", values_offset_end, Offset_end);
        if ((unsigned int)values_offset_end > Offset_end)
        {
            memset(pEncCfg->roi_file_name, 0x0, MAX_FILE_PATH);
        }
    }

    memset(sValue, 0x0, sizeof(sValue));
    if (WAVE_GetStringValue(fp, "reserved_str", (char *)&sValue, offset) == 0)
    {
        SAMPLE_PRT("parase the roi_file_name  error sValue = %s\n", sValue);
        memset(pEncCfg->reserved_str, 0x0, MAX_FILE_PATH);
        ///< goto __end_parse;
    }
    else
    {
        strcpy(pEncCfg->reserved_str, sValue);
        ///< pEncCfg->enPixelFormat = sValue;
    }

    fclose(fp);

    return ret;
}

void GetCfgOffset(FILE *fp, char *ptr, unsigned int count, int *offset)
{
    int VALUE = 0;
    int file_offset = 0;
    unsigned int i = 0;
    int temp_offset = 0;

    ///<SAMPLE_PRT( "ptr=%s,count = %d\n", ptr,count);

    for (i = 0; i < count; i++)
    {
        if (WAVE_GetValue(fp, ptr, &VALUE, temp_offset) == 0)
        {
            SAMPLE_PRT("error:VALUE = %d\n", VALUE);
            ///<			 temp_offset = ftell(fp);
        }
        else
        {
            ///<			SAMPLE_PRT( "VALUE = %d\n",VALUE );
            temp_offset = ftell(fp);
            ///<			*offset = temp_offset;
            ///<			break;
        }

        if ((unsigned int)VALUE == count)
        {
            file_offset = ftell(fp);
            if (file_offset >= 0)
            {
                *offset = file_offset;
            }
            break;
        }
    }

    return;
}

void GetValueCfgOffset(FILE *fp, char *ptr, unsigned int count, int offset, int *end_offset)
{
    int VALUE = 0;
    int file_offset = 0;

    if (WAVE_GetValue(fp, ptr, &VALUE, offset) == 0)
    {
        SAMPLE_PRT("error:ptr=%s VALUE = %d\n", ptr, VALUE);
    }

    if ((unsigned int)VALUE == count)
    {
        file_offset = ftell(fp);
        if (file_offset >= 0)
        {
            *end_offset = file_offset;
        }
    }

    return;
}

void GetStringCfgOffset(FILE *fp, char *ptr, char *dst_ptr, int offset, int *end_offset)
{
    int VALUE = 0;
    int file_offset = 0;
    char sValue[1024] = {0};

    if (WAVE_GetStringValue(fp, ptr, (char *)&sValue, offset) == 0)
    {
        SAMPLE_PRT("error:ptr=%s VALUE = %d\n", ptr, VALUE);
    }
    else
    {
        ///< temp_offset = ftell(fp);
    }

    SAMPLE_PRT("sValue=%s dst_ptr= %s\n", sValue, dst_ptr);
    if (strcmp(sValue, dst_ptr) == 0)
    {
        file_offset = ftell(fp);
        if (file_offset >= 0)
        {
            *end_offset = file_offset;
        }
    }

    return;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
