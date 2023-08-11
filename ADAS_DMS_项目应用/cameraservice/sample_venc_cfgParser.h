//------------------------------------------------------------------------------
// File: main_helper.h
//
// Copyright (c) 2006, Chips & Media.  All rights reserved.
//------------------------------------------------------------------------------
#ifndef _MAIN_HELPER_H_
#define _MAIN_HELPER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

# define	SEEK_SET	0
typedef void * osal_file_t;
#define MAX_CFG                 (178)


#define MAX_FILE_PATH               256
// for WAVE420
#define W4_MIN_ENC_PIC_WIDTH            256
#define W4_MIN_ENC_PIC_HEIGHT           128
#define W4_MAX_ENC_PIC_WIDTH            8192
#define W4_MAX_ENC_PIC_HEIGHT           8192

#define MAX_NUM_TEMPORAL_LAYER          7
#define MAX_GOP_NUM                     8

#define MAX_CFG                 (178)
#define MAX_ROI_LEVEL           (8)
#define LOG2_CTB_SIZE           (5)
#define CTB_SIZE                (1<<LOG2_CTB_SIZE)
#define LAMBDA_SCALE_FACTOR     (100000)
#define FLOATING_POINT_LAMBDA   (1)
#define TEMP_SCALABLE_RC        (1)
#define UI16_MAX                (0xFFFF)
#ifndef INT_MAX
#define INT_MAX                 (2147483647)
#endif

typedef struct {
    int u8MaxChnNum;
	int u8FlagCodeAndDecorder;
    int u32MaxPicWidth;
    int u32MaxPicHeight;
    int u32MaxPixelMode;
    int u32MaxTargetFrmRate;
    char VencFwPath[MAX_FILE_PATH];
    int VeChn;
    int BindChn;
    int BindChnBL;
    int venc_bind;
    int enType;
	int enGopMode;
    int frameRate;

    int u32BufSize;
    int u32Profile;
    int bByFrame;
    int u32PicWidth;

    int u32PicHeight;
    int enRcMode;
    char enPixelFormat[MAX_FILE_PATH];
    int u32PixelMode;
	int src_enFormat;
	int dst_enFormat;
    int intraRefreshMode;
    int u32Gop;

    int u32StatTime;
    int u32SrcFrmRate;
    int fr32TargetFrmRate;
    int u32BitRate;
    int u32FluctuateLevel;
    int u32MaxQp;

    int u32MinQp;
    int specialheader;
    int staicRefPerid;
    int u32IQp;
    int u32PQp;
    int u32BQp;
    int pool_id;
    int camBufCnt;
	
	int bcframeDataType;
	int bcframe50Enable;
	int bcframe50LosslessEnable;
	int cframe50Tx16Y;
	int cframe50Tx16C;

    int bEnableSvc;
	int u8SvcMode;
	int u8SvcRation;
	
    int bEnableTsvc;
	
    int u32Index;
    int bEnable;
    int bAbsQp;
    int s32Qp;
    int s32AvgOp;
    int s32Quality;
    int bBKEnable;
    int s32X;
    int s32Y;                      /**< It enables ROI map. NOTE: It is valid when rcEnable is on. */
    int u32Width;
    int u32Height;
	int chnnum;
	int runtime_sec;
    char streamname[MAX_FILE_PATH];
	int flag_2vdsp;
	int sensor_type;
	int enable_nosavefile;
	int dynamic_config_svc;	
	int dynamic_config_chn_flag;
	int dynamic_config_flag;
	int dynamic_config_num;
	int dymic_config_time_msleep;
	int chn_enable_flag;
	int enable_flag;
	int local_encode_flag;	
    char souce_filename[MAX_FILE_PATH];
    char souce_filenameBL[MAX_FILE_PATH];
	int souce_framenum;

	int bEnableLongTerm;
	int u32AsLongtermPeriod;
	int u32refLongtermPeriod;
	int EnSAO;
	int bEnableRotation;
	int u32rotAngle;
	int enmirDir;
	int RoiFlag;
	int EnRoi;
	char dowork_flag;
	char stopwork_flag;
    char roi_file_name[MAX_FILE_PATH];
	char ennable_nodebug;
	char test_async_mode;	
	int filesize_num;
	int reserved[4];//0:表示tftp保存一个文件的帧数 2,3,4预留
    char reserved_str[MAX_FILE_PATH];
    char u8SelectModeEnable;
    char u8SelectModeChn;
    char u8SelectMode;
    char u8IsUseNonVencFrameBuf;
} SAMPLE_ENC_CFG;


typedef struct {
    char  *name;
    char    property;/*0:number,1,characters*/
} CfgInfo;


int GetValue(void *fp, char *para, char *value);
int GetValues(void* fp, char *para, int *values, int num);
int WAVE_GetValue(void * fp,char* cfgName,int* value,unsigned int offset);
int WAVE_GetStringValue(void * fp, char* para,char* value,unsigned int offset);
int WAVE_GetValue_New(osal_file_t fp,char* cfgName,int* value,unsigned int offset,unsigned int offset_end);
int parseEncCfgFile( SAMPLE_ENC_CFG *pEncCfg, char *FileName,unsigned int offset,unsigned int Offset_end);
void printParsePara(SAMPLE_ENC_CFG *pEncCfg);
void GetCfgOffset(FILE * fp,char *ptr,unsigned int  count,int *offset);
int WAVE_GetValue_New(osal_file_t fp,char* cfgName,int* value,unsigned int offset,unsigned int offset_end);
void GetStringCfgOffset(FILE * fp,char *ptr,char *dst_ptr,int offset,int *end_offset);
void GetValueCfgOffset(FILE * fp,char *ptr,unsigned int  count,int offset,int *end_offset);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* _MAIN_HELPER_H_ */
 
