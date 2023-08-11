#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include "mpi_sys.h"
#include "mdvr_sample.h"
#include "sample_comm_vo.h"
#include "do_binder.h"
#include "do_ubinder_api.h"
#include "sample_comm_dcvi.h"
#include "mpi_vo.h"

#include "sample_common_vpss.h"
#include "mpi_venc.h"
#include "sample_venc_cfgParser.h"
#include "sample_comm_venc.h"
#include "sample_common_vpss_param.h"


/*mdvr add*/
#include <dirent.h>
#include <sys/statfs.h>
#include "osd.h"

/*mdvr add*/
#include <dirent.h>
#include <sys/statfs.h>
#include "VimRecordSdk.h"
#include "VimMediaType.h"
#include "vim_comm_region.h"
#include "sample_common_region.h"
#include "lc_glob_type.h"
#include "lc_dvr_record_log.h"
#include "lc_dvr_interface_4_vim.h"
#include "lc_codec_api.h"
#include "lc_jpeg_api.h"
#include "lc_dvr_comm_def.h"
#include "lc_dvr_glob_def.h"
#include "lc_dvr_build_sound_file.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif

#define CONFIG_MAX	8
extern 	VIM_S32 PARAM_GetManCtrlInfoFromIni(char *iniName , VIM_U32 Mask, VIM_U32 u32GrpId);
typedef struct __config_file
{
	VIM_U8   num;
	VIM_CHAR name[8][128];
}config_file;

typedef struct __sample_config
{
	config_file dcvi_config;
	config_file vo_config;
	config_file vpss_config;
	config_file venc_config;
}sample_config;

typedef struct menu_info
{
	VIM_S8 cmd;
    char *help_info;
    VIM_S32 (*handler)(void *param);
    VIM_VOID* param;
} VIEW_MENU_INFO_S;

#define VIM_DEBUG
#ifdef VIM_DEBUG
#define	SAMPLE_ENTER()			printf( "ENTER [%s %d]\n", __FUNCTION__,__LINE__ )
#define	SAMPLE_LEAVE()			printf( "LEAVE [%s %d]\n\n", __FUNCTION__,__LINE__)
#else
#define	SAMPLE_ENTER()			
#define	SAMPLE_LEAVE()			
#endif

#define VIEW_PAUSE(...) ({ \
    VIM_S32 ch1, ch2; \
    printf(__VA_ARGS__); \
    ch1 = getchar(); \
    if (ch1 != '\n' && ch1 != EOF) { \
        while((ch2 = getchar()) != '\n' && (ch2 != EOF)); \
    } \
    ch1; \
})
typedef struct release_vpssStruct
{
	VIM_VPSS_FRAME_INFO_S stFrameInfo;
	int vpssId;
}vpssBufStruct;
#define UNUSED_VARIABLE(x) ((void)(x))
#define DE_HD_PATH	"/dev/de-hd"


typedef struct _vpssFrameStruct_
{
	VIM_VPSS_FRAME_INFO_S frameInfo;
	int groupId;
	int chId;
}vpssFrameStruct;


/*mdvr add*/
int g_player_handle = -1;
MediaInfo g_media_Info;			/**当前播放视频的信息**/
int g_play_time_current;		/**当前的播放进度**/
int g_play_status_current;		/**播放状态(1) 暂停状态(0) 结束状态(-1)**/
VIM_BOOL g_player_show = VIM_FALSE;
VIM_BOOL g_player_flag = VIM_FALSE;
int playStartEvent = 0;
VIM_BOOL play_Exit = VIM_FALSE;
char playerFilePath[256] = {0};
int g_handle = -1;
unsigned char dcviChannelMask = 0;
unsigned char pwdMd5Str[34] = {0};
MediaInfo recInfo;
static pthread_mutex_t g_rtspMutex;
static int g_rtspStartCount = 0;

/**中星微录像方式的宏定义**/
//#define VIM_RECORD_PATH
//#define VIM_WITH_PICTURE

#define PACK_H265    /*打开此宏使用H265编码, 否则使用SVAC编码*/
#define RECORD_DEBUG
//#define PREFIX_PATH         "/mnt/nfs"
#define PREFIX_PATH         "/mnt/usb/usb1"
#define RECORD_DIR_PATH     PREFIX_PATH "/record"
#define ALARM_DIR_PATH      PREFIX_PATH "/record/alarm"
#define VIDEO_DIR_PATH      PREFIX_PATH "/record/video"
#define CHANNEL_DIR_PATH    PREFIX_PATH "/record/video/ch"

#define LC_VC_384x384_CHANEL   4
#define LC_VC_1920x1080_CHANEL 0
#define LC_VC_960X544_CHANEL   2
#define LC_VC_1280X720_CHANEL  3
#define LC_VC_VENC_CHANEL1 	   1  /**中星微编码通道**/
#define LC_ALGORITEM_SIZE	   384
#define LC_ALGORITEM_SIZE_ADAS_width	   640
#define LC_ALGORITEM_SIZE_ADAS_height	   352


#define LC_HAVE_ALGORITHEM

static pthread_rwlock_t g_vo_rwlock;
static VIM_BOOL View_Exit = VIM_FALSE;
static int Dms_Switch = 0;    //DMS 鍔熻兘寮�鍏?static int Adas_Switch = 1;   //ADAS鍔熻兘寮�鍏? 
static const char short_options[] = "hc:o:m:v:r:a:t:p:b:e:g:q:n:s:d:f:i:z:y:x:u";
int mainPreview = 0;
int isSetSize = 0;
int showChan = 0;

static const struct option long_options[] = 
{
   	{ "help", no_argument, NULL, 'h' }, 
   	{ "normal", no_argument, NULL, 'n' },
   	{ "channel_num", 1, NULL, 'c' },  
   	{ "set_path", no_argument, NULL, 'p' },
	{ 0, 0, 0, 0 }
};
static VB_CONF_S gVbConfig = {
	.u32MaxPoolCnt = 0,
};

SAMPLE_MAN_INFO_S mstManInfo;
VIM_VPSS_GROUP_ATTR_S  grpAttr[16];
VIM_VPSS_INPUT_ATTR_S  chnInAttr[16];
VIM_CHN_CFG_ATTR_S     chnOutAttr[16][MAX_VPSS_CHN];

static const int g_frameListSize = 32;
static vpssFrameStruct g_vpssFrameList[32];
static pthread_mutex_t g_frameListMutex;
static char* gDmsAlgorithemImgList[4];
static char* gAdasAlgorithemImgList[4];
static int gDmsImgIndexHead, gDmsImgIndexTail;
static int gAdasImgIndexHead, gAdasImgIndexTail;
static int g_framelistLen = 0;
static int g_framelistMax = 0;
static int mplayTimes = 0;
static sem_t semAlgorithem;
static int mDmsFrameRate;
static int mAdasFrameRate;


int mFrameCnt = 0;
int mDecodeFrameCnt = 0;
int mCameraLossDetect = 0;

void *mediaPlay_thread(void *arg);
VIM_S32 vc_record_start(void);
//--------------------------interface define------------------------------------
extern int vim_record_filePath(int ch, int event, char* fullpath);
extern void vc_message_send(int eMsg, int val, void* para, void* func);
extern char* RecordGetLicense(void);
extern int RecordGetCurRecordInfoIndex(int type, int chanel);
extern int RecordGetCurSoundIndex(void);
extern int vc_record_video_para_get(int index);
//------------------------------------------------------------------------------
void vc_record_start_again(void)
{
    for(int recCh = 0; recCh < 4; recCh++)
    {    	
		int s32Ret = VimRecordChStop(recCh);
        if(s32Ret != 0)
        {
            logd_lc("error:VimRecordChStop ch:%d ret:%d\n", recCh, s32Ret);
        }		
    }
	vc_record_start();
}

void vc_record_video_para_config_update(void)
{	
	int photoSize = vc_record_video_para_get(VC_IDX_PHOTOSZ);
	recInfo.duration = vc_record_video_para_get(VC_IDX_DURATION);
	recInfo.bitRate = vc_record_video_para_get(VC_IDX_BITRATE);
	recInfo.aFmt = vc_record_video_para_get(VC_IDX_MUTE);
	if(photoSize==1)
	{
		recInfo.vInfo.width = LC_RECORD_VIDEO_NSIZE_W; //1920;//3840;
		recInfo.vInfo.height = LC_RECORD_VIDEO_NSIZE_H; //1080;//2160;
	}
	else
	{
		recInfo.vInfo.width = LC_RECORD_VIDEO_HSIZE_W; //1920;//3840;
		recInfo.vInfo.height = LC_RECORD_VIDEO_HSIZE_H; //1080;//2160;		
	}
}

static void usage(FILE * fp, VIM_S32 argc, char ** argv) 
{  
	UNUSED_VARIABLE(fp);
	UNUSED_VARIABLE(argc);

	printf("Usage: %s [options]\n\n"
			"Options:\n"
			"-h | --help Print this message.\n"
			"-a | --specify sif_isp_config\n"
			"-v | --specify vpss_config \n"
			"-o | --vo :set  the vo cfg  file name\n"
			"", argv[0]);
} 

VIM_S32 View_Sample_Quit(void *param)
{
	VIM_S32 s32Ret = 0;
	
	View_Exit = VIM_TRUE;
	
	return s32Ret;
}

VIEW_MENU_INFO_S view_menus[] = {
	{'q', "quit", View_Sample_Quit, NULL},
	// {'c',"snap sif frame",sif_snap_trigger,NULL},
};

void SAMPLE_VIEW_Print_Menu(VIEW_MENU_INFO_S *pstmenu_info,VIM_S32 memu_num)
{
    VIM_S32 i;

    for (i = 0; i < memu_num; i++){
        printf("   press [%c] to %s\n", pstmenu_info[i].cmd, pstmenu_info[i].help_info);
    }
}


void SAMPLE_VIEW_Handle_Menu(VIEW_MENU_INFO_S *pstmenu_info)
{
	VIM_S32 memu_num  =sizeof(view_menus)/sizeof(VIEW_MENU_INFO_S);
    char c = 0;
    VIM_S32 i = 0;

    while (View_Exit == VIM_FALSE){
		printf("\nhelp:\n");
        SAMPLE_VIEW_Print_Menu(pstmenu_info,memu_num);

        c = VIEW_PAUSE(" ");

        for(i = 0; i< memu_num;i++){
			if (c != pstmenu_info[i].cmd)
				continue;
			
            if (pstmenu_info[i].handler == NULL){
                break;
            }
            pstmenu_info[i].handler(pstmenu_info[i].param);
        }
    }
}

static VIM_S32 SAMPLE_COMM_SYS_Init(VB_CONF_S *pstVbConf)
{
	MPP_SYS_CONF_S stSysConf = {0};
	VIM_S32 s32Ret = VIM_FAILURE;

	SAMPLE_ENTER();

	VIM_MPI_SYS_Exit();
	VIM_MPI_VB_Exit();

	if ( NULL == pstVbConf ){
		SAMPLE_PRT( "input parameter is null, it is invaild!\n" );
		return VIM_FAILURE;
	}

	s32Ret = VIM_MPI_VB_SetConf( pstVbConf );

	if ( VIM_SUCCESS != s32Ret ){
		SAMPLE_PRT( "VIM_MPI_VB_SetConf failed!\n" );
		return VIM_FAILURE;
	}

	s32Ret = VIM_MPI_VB_Init();

	if ( VIM_SUCCESS != s32Ret ){
		SAMPLE_PRT( "VIM_MPI_VB_Init failed!\n" );
		return VIM_FAILURE;
	}

	stSysConf.u32AlignWidth = 32;
	s32Ret = VIM_MPI_SYS_SetConf( &stSysConf );

	if ( VIM_SUCCESS != s32Ret ){
		SAMPLE_PRT( "VIM_MPI_SYS_SetConf failed\n" );
		return VIM_FAILURE;
	}

	s32Ret = VIM_MPI_SYS_Init();

	if ( VIM_SUCCESS != s32Ret ){
		SAMPLE_PRT( "VIM_MPI_SYS_Init failed!\n" );
		return VIM_FAILURE;
	}
	
	SAMPLE_LEAVE();
	return VIM_SUCCESS;
}
static VIM_VOID SAMPLE_COMM_SYS_Exit(void)
{
    VIM_MPI_SYS_Exit();
    VIM_MPI_VB_Exit();
    return;
} 

VIM_S32 ToolsBind_DCVI_VO_Start(void)
{
	VIM_S32 s32Ret = 0;
    VIM_U32 type=0;
    SYS_BD_CHN_S srcchn[4]={0}, dstchn={0};
    srcchn[0].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[0].s32DevId=0x1;
    srcchn[0].s32ChnId=0x0;

    srcchn[1].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[1].s32DevId=0x1;
    srcchn[1].s32ChnId=0x1;

    srcchn[2].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[2].s32DevId=0x1;
    srcchn[2].s32ChnId=0x2;

    srcchn[3].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[3].s32DevId=0x1;
    srcchn[3].s32ChnId=0x3;

    dstchn.enModId=VIM_BD_MOD_ID_VOU;
    dstchn.s32DevId=0x1;
    dstchn.s32ChnId=0;
    type=VIM_BIND_WK_TYPE_DRV;
    s32Ret=VIM_MPI_SYS_Bind(4,&srcchn[0], 1,&dstchn, type);
	if(s32Ret != 0)
		printf("DCVI VO VIM_MPI_SYS_Bind fail.\n");
	printf("function: %s line:%d s32Ret:%d.\n", __func__, __LINE__,s32Ret);
	return s32Ret;
}

VIM_S32 ToolsBind_DCVI_VO_Stop(void)
{
	VIM_S32 s32Ret = 0;
    VIM_U32 type=0;
    SYS_BD_CHN_S srcchn[4]={0}, dstchn={0};
    srcchn[0].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[0].s32DevId=0x1;
    srcchn[0].s32ChnId=0x0;

    srcchn[1].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[1].s32DevId=0x1;
    srcchn[1].s32ChnId=0x1;

    srcchn[2].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[2].s32DevId=0x1;
    srcchn[2].s32ChnId=0x2;

    srcchn[3].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[3].s32DevId=0x1;
    srcchn[3].s32ChnId=0x3;

    dstchn.enModId=VIM_BD_MOD_ID_VOU;
    dstchn.s32DevId=0x1;
    dstchn.s32ChnId=0;
    type=VIM_BIND_WK_TYPE_DRV;
    s32Ret=VIM_MPI_SYS_UnBind(4,&srcchn[0], 1,&dstchn, type);
	if(s32Ret != 0)
		printf("DCVI VO VIM_MPI_SYS_UnBind fail.\n");
	printf("function: %s line:%d s32Ret:%d.\n", __func__, __LINE__,s32Ret);
	return s32Ret;
}

VIM_S32 ToolsBind_DCVI_VPSS_Start(void)
{
	VIM_S32 s32Ret = 0;
    VIM_U32 type=0;
    SYS_BD_CHN_S srcchn[4]={0}, dstchn={0};
    srcchn[0].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[0].s32DevId=0x1;
    srcchn[0].s32ChnId=0x0;

    srcchn[1].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[1].s32DevId=0x1;
    srcchn[1].s32ChnId=0x1;

    srcchn[2].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[2].s32DevId=0x1;
    srcchn[2].s32ChnId=0x2;

    srcchn[3].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[3].s32DevId=0x1;
    srcchn[3].s32ChnId=0x3;

    dstchn.enModId=VIM_BD_MOD_ID_VPSS;
    dstchn.s32DevId=VIM_VPSS_BIND_DRVID;
    dstchn.s32ChnId=VPSS_BIND_INPUT_0;
    type=VIM_BIND_WK_TYPE_DRV;

    s32Ret=VIM_MPI_SYS_Bind(4,&srcchn[0], 1,&dstchn, type);
	if(s32Ret != 0)
		printf("DCVI VO VIM_MPI_SYS_Bind fail.\n");

	printf("function: %s line:%d s32Ret:%d.\n", __func__, __LINE__,s32Ret);
	return s32Ret;
}

static int setVpssFrameList(VIM_VPSS_FRAME_INFO_S* frame, int groupId, int chId)
{
    pthread_mutex_lock(&g_frameListMutex);
    for(int i = 0; i < g_frameListSize; i++)
    {
        if(g_vpssFrameList[i].frameInfo.pPhyAddrStd[0] == NULL)
        {
            g_vpssFrameList[i].groupId = groupId;
            g_vpssFrameList[i].chId = chId;
            memcpy(&(g_vpssFrameList[i].frameInfo), frame, sizeof(VIM_VPSS_FRAME_INFO_S));
            g_framelistLen++;
            if(g_framelistLen > g_framelistMax)
            {
                g_framelistMax = g_framelistLen;
                printf("vpssFrameListLen-----------------------max:%d\n", g_framelistMax);
            }
            pthread_mutex_unlock(&g_frameListMutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_frameListMutex);
    printf("setVpssFrameList-------------------list is full!!!:addr:%x,groupId:%d, chId:%d!!!\n", frame->pPhyAddrStd[0], groupId, chId);
    return 1;
}

static int getVpssFrameList(VIM_U32 addr, VIM_VPSS_FRAME_INFO_S* frame, int* pGroupId, int* pChId)
{
    pthread_mutex_lock(&g_frameListMutex);
    for(int i = 0; i < g_frameListSize; i++)
    {
        if(g_vpssFrameList[i].frameInfo.pPhyAddrStd[0] == addr)
        {
            if(frame != NULL)
            {
                memcpy(frame, &(g_vpssFrameList[i].frameInfo), sizeof(VIM_VPSS_FRAME_INFO_S));
                *pGroupId = g_vpssFrameList[i].groupId;
                *pChId = g_vpssFrameList[i].chId;
            }
            g_vpssFrameList[i].frameInfo.pPhyAddrStd[0] = NULL;
            g_framelistLen--;
            pthread_mutex_unlock(&g_frameListMutex);
            return 0;
        }
    }
    printf("getVpssFrameList-------------not find frame addr:%x,listlen:%d!!!\n", addr, g_framelistLen);
    pthread_mutex_unlock(&g_frameListMutex);

    return 1;
}

static int queryVpssFrameList(int group)
{
    int number = 0;
    pthread_mutex_lock(&g_frameListMutex);
    for(int i = 0; i < g_frameListSize; i++)
    {
        if(g_vpssFrameList[i].frameInfo.pPhyAddrStd[0] != NULL)
        {
            if(g_vpssFrameList[i].groupId == group)
            {
                pthread_mutex_unlock(&g_frameListMutex);
                return 1;
            }
            number++;
            if(g_framelistLen == number)
            {
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_frameListMutex);
    return 0;
}


static void releaseVpssFrameList(int groupId, int chId)
{
    while(1)
    {
        int getOne = 0;
        VIM_VPSS_FRAME_INFO_S frame;
        pthread_mutex_lock(&g_frameListMutex);
        for(int i = 0; i < g_frameListSize; i++)
        {
            if(g_vpssFrameList[i].frameInfo.pPhyAddrStd[0] != NULL && g_vpssFrameList[i].groupId == groupId && g_vpssFrameList[i].chId == chId)
            {
                memcpy(&frame, &(g_vpssFrameList[i].frameInfo), sizeof(VIM_VPSS_FRAME_INFO_S));
                g_vpssFrameList[i].frameInfo.pPhyAddrStd[0] = NULL;
                g_framelistLen--;
                getOne = 1;
                break;
            }
        }
        pthread_mutex_unlock(&g_frameListMutex);
        if(getOne == 0)
        {
            break;
        }
        VIM_MPI_VPSS_GetFrame_Release(groupId, chId, &frame);
        printf("clearVpssFrameList-----------------relase frame:%d,%d,%x\n", groupId, chId, frame.pPhyAddrStd[0]);
    }
}

int lc_AudioFrameCb(const AFrameInfo* info, const unsigned char* data, int len, long long pts, void* userData)
{
	lc_dvr_sound_data_insert(data, len);
	return 0;
}

void sample_feed_vo(void *chan)
{
	int ret = 0;
	int vpssId = 5;//0原始分辨率 5缩小
	int continueSign;
	showChan = 0;//用于轮询的通道变量
	MultiChanBind Vo_MultiChanInfo = {0};
	voImageCfg setStride = {0};
	VIM_VPSS_FRAME_INFO_S stFrameInfo = {0};
	Vo_MultiChanInfo.high = 540;
	Vo_MultiChanInfo.width = 960;
	Vo_MultiChanInfo.chan = showChan;
	Vo_MultiChanInfo.workMode = Multi_2X2;
	vpssId = *((int*)chan);
	printf("sample_feed_vo~ pthread success chan %d\n",vpssId);
	if(LC_VC_1920x1080_CHANEL == vpssId)
	{//原始分辨率
		while(1)
		{
			continueSign = 0;
			if(VIM_TRUE == View_Exit)
			{
				break;
			}
			
			if(0 != mainPreview) // mainPreview == 1为1080预览
			{
				if(0 == isSetSize)
				{
					isSetSize = 1;
					VIM_MPI_VO_SetMultiMode(Multi_None,MULTI_CONTROL_AUTO,NULL,NULL);
					printf("change MultiMode Multi_None[%d].\n", __LINE__);
					Vo_MultiChanInfo.workMode = Multi_None;
					setStride.chan = 0;
					setStride.LayerId = 0;
					setStride.voDev = 1;
					setStride.weight = 1920;
					VIM_MPI_VO_SetImageSize(&setStride,1);
				}
			}
			else
			{
				 continueSign = 1;
			}
		
			ret = VIM_MPI_VPSS_GetFrame(0, vpssId, &stFrameInfo, 3000);

			if(ret == 0)
	        {
	        	for(int chl=0; chl<4; chl++)
	        	{
		        	LC_VENC_STREAM_INFO *streamCtr = vc_stream_venc_resolution_get(chl);					
		        	if(streamCtr->on_off)
		        	{/**对选定通道的图像进行编码**/
		        		if((streamCtr->resolution == STREAM_1920X1080_ENU)&&(stFrameInfo.u32SrcVidChn == streamCtr->camChnl))
		        		{        		
		        			lc_venc_sendFrame_getStream(streamCtr->codeChnl, &stFrameInfo); /**推流用通道**/
		        		}
		        	}
	        	}
				
				JPEG_PIC_CTRL_H *jpgCtr = lc_jpeg_handler(LC_LARGE_ENCODE_CHANEL);
				if((jpgCtr->u32UsedSign)&&(jpgCtr->cam_chn == stFrameInfo.u32SrcVidChn))
				{
					if(jpgCtr->do_TakePic)
					{
						lc_jpeg_frame_send(&stFrameInfo, LC_LARGE_ENCODE_CHANEL);
						jpgCtr->do_TakePic=0;
					}
				}
				
				if(continueSign)
				{
					VIM_MPI_VPSS_GetFrame_Release(0,vpssId,&stFrameInfo);
					usleep(1);
					continue;
				}				
	        }
			else
			{
				printf("VIM_MPI_VPSS_GetFrame %d err %x\n",vpssId,ret);
				usleep(1);
				continue;
			}				

			{
				Vo_MultiChanInfo.chan = stFrameInfo.u32SrcVidChn;
				Vo_MultiChanInfo.codeAddr_y =(char*)stFrameInfo.pPhyAddrStd[0];
				Vo_MultiChanInfo.codeAddr_uv =(char*)stFrameInfo.pPhyAddrStd[1];
			}

		    int bRelease = 1;
		    pthread_rwlock_rdlock(&g_vo_rwlock);
			if((0 != mainPreview)&&(showChan == Vo_MultiChanInfo.chan) && (VIM_FALSE == g_player_flag))
			{//不分块并且get的是想显示的通道
				Vo_MultiChanInfo.chan = 0;
				//printf("inqueue[%d]==================group:%d, chan:%d, vpssid:%d,  pPhyAddrStd=%#x.\n", __LINE__, 0, showChan, vpssId, Vo_MultiChanInfo.codeAddr_y);
                setVpssFrameList(&(stFrameInfo), 0, vpssId);
				ret = VIM_MPI_VO_SetFrameA(&Vo_MultiChanInfo,1);
            	if(ret == VIM_SUCCESS)
				{
					bRelease = 0;
				}
				else
				{
					getVpssFrameList(Vo_MultiChanInfo.codeAddr_y, NULL, NULL, NULL);
				}
			}
            pthread_rwlock_unlock(&g_vo_rwlock);
            if(bRelease)
            {
				VIM_MPI_VPSS_GetFrame_Release(0,vpssId,&stFrameInfo);
            }

	  		usleep(1);
		}
	}
	else if (LC_VC_960X544_CHANEL == vpssId)
	{
		while(1)
		{
			continueSign = 0;
			if(VIM_TRUE == View_Exit)
			{
				break;
			}
			if (0 == mainPreview)
			{//分块 缩小尺寸
				if(0 == isSetSize)
				{
					VIM_MPI_VO_SetMultiMode(Multi_2X2,MULTI_CONTROL_AUTO,NULL,NULL);
					printf("change MultiMode Multi_2X2[%d].\n", __LINE__);
					Vo_MultiChanInfo.workMode = Multi_2X2;
					setStride.chan = 0;//只有0通道被修改，其它通道默认是分块的尺寸，这里只需要重置0通道就行了
					setStride.LayerId = 0;
					setStride.voDev = 1;
					setStride.weight = 960;
					VIM_MPI_VO_SetImageSize(&setStride,1);
					isSetSize = 1;
				}
			}
			else
			{
				 continueSign = 1;
			}
		
			ret = VIM_MPI_VPSS_GetFrame(0, vpssId, &stFrameInfo, 3000);

			if(ret == 0)
			{
				//========================推流数据获取===========================
				for(int chl=0; chl<4; chl++)
				{
					LC_VENC_STREAM_INFO *streamCtr = vc_stream_venc_resolution_get(chl);	
					if(streamCtr->on_off)
					{/**对选定通道的图像进行编码**/
						if((streamCtr->resolution == STREAM_960X544_ENU)&&(stFrameInfo.u32SrcVidChn == streamCtr->camChnl))
						{
							lc_venc_sendFrame_getStream(streamCtr->codeChnl, &stFrameInfo); /**推流用通道**/
						}
					}	
				}

				JPEG_PIC_CTRL_H *jpgCtr = lc_jpeg_handler(LC_STREAM_ENCODE_CHANEL);
				if((jpgCtr->u32UsedSign)&&(jpgCtr->cam_chn == stFrameInfo.u32SrcVidChn))
				{
					if(jpgCtr->do_TakePic)
					{
						lc_jpeg_frame_send(&stFrameInfo, LC_STREAM_ENCODE_CHANEL);
						jpgCtr->do_TakePic=0;
					}
				}			
				//===============================================================
				if(continueSign)
				{
					VIM_MPI_VPSS_GetFrame_Release(0,vpssId,&stFrameInfo);
					usleep(1);
					continue;
				}				
			}
			else
			{
				printf("VIM_MPI_VPSS_GetFrame %d err %x\n",vpssId,ret);
				usleep(1);
				continue;
			}

			{
				Vo_MultiChanInfo.chan = stFrameInfo.u32SrcVidChn;
				Vo_MultiChanInfo.codeAddr_y =(char*) stFrameInfo.pPhyAddrStd[0];
				Vo_MultiChanInfo.codeAddr_uv = (char*)stFrameInfo.pPhyAddrStd[1];
			}

            int bRelease = 1;
		    pthread_rwlock_rdlock(&g_vo_rwlock);
			//分块则get到的流都显示
			if ((0 == mainPreview) && (VIM_FALSE == g_player_flag))
			{
				//printf("inqueue[%d]==================group:%d, chan:%d, vpssid:%d,  pPhyAddrStd=%#x.\n", __LINE__, 0, Vo_MultiChanInfo.chan, vpssId, Vo_MultiChanInfo.codeAddr_y);
                setVpssFrameList(&(stFrameInfo), 0, vpssId);
				ret = VIM_MPI_VO_SetFrameA(&Vo_MultiChanInfo,1);
            	if(ret == VIM_SUCCESS)
				{
					bRelease = 0;
				}
				else
				{
					getVpssFrameList(Vo_MultiChanInfo.codeAddr_y, NULL, NULL, NULL);
				}
			}
			pthread_rwlock_unlock(&g_vo_rwlock);
            if (bRelease)
            {
				VIM_MPI_VPSS_GetFrame_Release(0,vpssId,&stFrameInfo);
            }
	  		usleep(1);
		}
	}
	else
	{
	
	}
}

/**Stream用通道1280x720**/
void *vc_channel3_handler(void *arg)
{
    int ret = -1;
	VIM_VPSS_FRAME_INFO_S stFrameInfo = {0};

	logd_lc("vc_channel3_handler\n");	
    while (VIM_FALSE == View_Exit)
    {
        ret = VIM_MPI_VPSS_GetFrame(0, LC_VC_1280X720_CHANEL, &stFrameInfo, 3000);

        if(0 != ret)
        {
            logd_lc("CHL3 VPSS_GetFrame err %x\n", ret);
            usleep(1);
            continue;
        }
        else
        {			
        	if(stFrameInfo.u32SrcVidChn == 0)
        	{
        		vc_loop_image_handle();
				if(mCameraLossDetect)
				{
					mCameraLossDetect--;
					if(mCameraLossDetect == 0)
					{
						int cStatus = vc_get_record_status();
						int ret;
						for(int i=0; i<4; i++)
						{
							if((cStatus&1<<i)==0)
							{
								ret = VimRecordChStop(i);
								if(ret != 0)
								{
									printf("error:VimRecordChStop ch:%d ret:%d\n", i, ret);
								}								
							}
						}						
						vc_message_send(ENU_LC_DVR_CAM_DEV_ERROR, cStatus, NULL, NULL);
					}
				}
        	}
			
			for(int chl=0; chl<4; chl++)
			{
				LC_VENC_STREAM_INFO *streamCtr = vc_stream_venc_resolution_get(chl);
	        	if(streamCtr->on_off)
	        	{/**对选定通道的图像进行编码**/
	        		if((streamCtr->resolution == STREAM_1280X720_ENU)&&(stFrameInfo.u32SrcVidChn == streamCtr->camChnl))
	        		{
	        			lc_venc_sendFrame_getStream(streamCtr->codeChnl, &stFrameInfo); /**推流用通道**/
	        		}
	        	}

				JPEG_PIC_CTRL_H *jpgCtr = lc_jpeg_handler(LC_SMALL_ENCODE_CHANEL);
				if((jpgCtr->u32UsedSign)&&(jpgCtr->cam_chn == stFrameInfo.u32SrcVidChn))
				{
					if(jpgCtr->do_TakePic)
					{
						logd_lc("take pic 1280X720_ENU");
						lc_jpeg_frame_send(&stFrameInfo, LC_SMALL_ENCODE_CHANEL);
						jpgCtr->do_TakePic = 0;
		        	}
				}

				jpgCtr = lc_jpeg_handler(LC_THUMB_ENCODE_CHANEL);
				if((jpgCtr->u32UsedSign)&&(jpgCtr->cam_chn == stFrameInfo.u32SrcVidChn))
				{
					if(jpgCtr->do_TakePic)
					{
						if(jpgCtr->frameDelay==0)
						{
							logd_lc("take pic 640X384_ENU");
							lc_scaler_picture_take(1280, 720, &stFrameInfo);
							jpgCtr->do_TakePic = 0;
						}
						else
						{
							jpgCtr->frameDelay--;
						}
					}
				}	
	        }
		}
			
		VIM_MPI_VPSS_GetFrame_Release(0, LC_VC_1280X720_CHANEL, &stFrameInfo);
        usleep(1);		
#if 0
	mFrameCnt++;
	if(mFrameCnt == 800)
	{
		LC_DVR_RECORD_MESSAGE *msgSend = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(64);
		int *fileID = msgSend->msg_buf;
		memset(msgSend, 0, 64);
		fileID[0] = Record_build_file_ID(LC_NORMAL_VIDEO_RECORD, 2, 12);
		msgSend->event = ENU_LC_DVR_FETCH_FILE_BY_ID;
		sys_msg_queu_in(msgSend);		
	}
	
	if(mFrameCnt == 1000)
	{
		LC_DVR_RECORD_MESSAGE *msgSend = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(sizeof(LC_DVR_RECORD_MESSAGE));
		LC_DVR_LOOP_IMAGE_TAKE *picInfo = (LC_DVR_LOOP_IMAGE_TAKE*)msgSend->msg_buf;

		msgSend->event = ENU_LC_DVR_IMAGE_LOOP_TAKE;
		memset(picInfo, 0, sizeof(LC_DVR_LOOP_IMAGE_TAKE));
		picInfo->imageSize = 2;
		picInfo->chnl_en[0] = 1;
		picInfo->chnl_en[1] = 1;
		picInfo->interval = 120;
		sys_msg_queu_in(msgSend);
	}
	
	if(mFrameCnt == 2400)
	{
	  /** 录像回放测试，播放视频文件 **/
		logd_lc("file play startxxx (%d)!", mplayTimes++);
		char *filePath = (char*)sys_msg_get_memcache(128);
		sprintf(filePath, "/mnt/sdcard/TestVideoH265_1080.MP4");
		vc_message_send(ENU_LC_DVR_SPECIAL_EMERGENCY_VIDEO, 120, NULL, NULL);

		vc_message_send(ENU_LC_DVR_SOUND_RECORD_START, (120|0x8000), NULL, NULL);
//		vc_message_send(ENU_LC_DVR_VIDEO_RECORD_RESTART, 0, filePath, NULL);
//		vc_message_send(ENU_LC_DVR_MEDIA_PLAY_START, 0, filePath, NULL);

//		mFrameCnt = 600;
    }
	else if(mFrameCnt == 3000)
	{
		// vc_message_send(ENU_LC_DVR_FETCH_CUR_EMERGENCY, (LC_SPECIAL_EMERGNECY_EVENT<<8)|1, NULL, NULL);

		vc_message_send(ENU_LC_DVR_FETCH_CUR_LOOP_IMAGE, 0, NULL, NULL);
		logd_lc("fetch emergency cur file");
	}
	else if(mFrameCnt == 3200)
	{
		LC_DVR_RECORD_MESSAGE *message = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(sizeof(LC_DVR_RECORD_MESSAGE));		
		LC_RECORD_LIST_FETCH *fileFetch = (LC_RECORD_LIST_FETCH*)message->msg_buf;
		
		message->msg_id = MSG_BUILD(LC_DVR_SYS_SERID, LC_DVR_VIDEO_SERID);
		message->event = ENU_LC_DVR_FETCH_LOOP_IMAGE_LIST;
		message->value = 0; 
		fileFetch->fileType = LC_SPECIAL_EMERGNECY_EVENT;
		fileFetch->camChnl = 0;
		fileFetch->offset = RecordGetCurRecordInfoIndex(LC_LOOP_IMAGE_RECORD, 0)-1;
		if(fileFetch->offset < 0 ) fileFetch->offset = 0;
		fileFetch->fileNumber = 8;
		sys_msg_queu_in(message);	
		logd_lc("fetch emergency file list");
	}	

	
//	else if(mFrameCnt == 1200)
//	{
//		if(g_handle>=0)
//		{
//			int playSpeed = 2;
//			int retval = VimPlayerCtrl(g_handle, PLAY_SPEED_CMD, &playSpeed);
//			logd_lc("xxx PLAY_SPEED_CMD(%d)-->%d", playSpeed, retval);
//		}
//	}
#endif

#ifdef LC_MDVR_FUNCTION_TEST				
		if(stFrameInfo.u32SrcVidChn == 2) 
		{
			mFrameCnt++;
//			if(mFrameCnt==550)		
//        	{
//				LC_TAKE_PIC_INFO *picInfo = (LC_TAKE_PIC_INFO*)sys_msg_get_memcache(256);		
//				picInfo->pic_width = LC_TAKE_LARGE_PIC_SIZE_W;
//				picInfo->pic_height = LC_TAKE_LARGE_PIC_SIZE_H;
//				picInfo->camChnl = 3; /**拍照的摄像头通道选择**/
//				sprintf(picInfo->pic_save_path, "/mnt/usb/usb1/WondPic1088.jpg"); 
//				vc_message_send(ENU_LC_DVR_MDVR_TAKEPIC, 0, picInfo, NULL);
//			}
//			else if(mFrameCnt==600)		
//			{
//				LC_TAKE_PIC_INFO *picInfo = (LC_TAKE_PIC_INFO*)sys_msg_get_memcache(256);				
//				picInfo->pic_width = LC_TAKE_STREAM_PIC_SIZE_W;
//				picInfo->pic_height = LC_TAKE_STREAM_PIC_SIZE_H;
//				picInfo->camChnl = 3; /**拍照的摄像头通道选择**/
//				sprintf(picInfo->pic_save_path, "/mnt/usb/usb1/WondPic544.jpg");
//				vc_message_send(ENU_LC_DVR_MDVR_TAKEPIC, 0, picInfo, NULL);
//			}
			

			if(mFrameCnt==500)
			{ 
			  /** 录像回放测试，播放视频文件 **/
				logd_lc("file play start (%d)!", mplayTimes++);
				char *filePath = (char*)sys_msg_get_memcache(128);
				// sprintf(filePath, "/mnt/sd/leftVideo2/003-GBT19056_QB84936_2_20230209104156.mp4");
				sprintf(filePath, "/mnt/usb/usb1/test_video_file.mp4");
				vc_message_send(ENU_LC_DVR_MEDIA_PLAY_START, 0, filePath, NULL);			
			}

//			if(mFrameCnt==360)
//			{
//				char *filePath = (char*)sys_msg_get_memcache(128);
//				sprintf(filePath, "/mnt/usb/usb2");
//				vc_message_send(ENU_LC_DVR_GB19056_VIDEO_COPY, 0, filePath, NULL);			
//			}		
			
//			if(mFrameCnt==120)
//			{
//				vc_message_send(ENU_LC_DVR_MSG_FOR_TEST, 9, NULL, NULL);
//			}
			
//			if(mFrameCnt==360)
//			{
//				vc_message_send(ENU_LC_DVR_MSG_FOR_TEST, 1, NULL, NULL);
//			}
//			else if(mFrameCnt==480)
//			{
//				vc_message_send(ENU_LC_DVR_MSG_FOR_TEST, 1, NULL, NULL);
//			}	
//			else if(mFrameCnt==500)
//			{
//				vc_message_send(ENU_LC_DVR_MSG_FOR_TEST, 1, NULL, NULL);
//			}
			
//			if(mFrameCnt == 200)
//			{
//				char *filePath = (char*)sys_msg_get_memcache(128);
//				strcpy(filePath, "/mnt/nfs/media/007-GBT19056_YA74576_2_20220929004753.mp4");
//				vc_message_send(ENU_LC_DVR_MEDIA_PLAY_START, 0, filePath, NULL);
//			}
//			else if(mFrameCnt == 400)
//			{
//				mFrameCnt = 0;
//				vc_message_send(ENU_LC_DVR_MEDIA_PLAY_STOP, 0, NULL, NULL);
//			}
			
//			if((mFrameCnt&0xff) == 200)
//			{						
//				//vc_message_send(ENU_LC_DVR_DISPLAY_CHANEL, mDisplayChnl, NULL, NULL);
//				mDisplayChnl++;
//				if(mDisplayChnl==5) mDisplayChnl = 0;

//				vc_message_send(ENU_LC_DVR_MSG_FOR_TEST, mDisplayChnl, NULL, NULL); /**???????????б????*/	
//			}
		}
#endif 		
	}	
	
	logd_lc("vc_chanel3 Exit!\n");
    return NULL;
}

#ifdef LC_HAVE_ALGORITHEM
void *vc_algorithem_handler(void *arg)
{
	struct timeval start, end;	
	int interval, temp;
	int result;	

	logd_lc("vc_algorithem_handler run!");
	for(;;)
	{
		sem_wait(&semAlgorithem);
		
		if(gDmsImgIndexTail!=gDmsImgIndexHead && Dms_Switch)
		{
			int index = gDmsImgIndexHead&0x03;
			gettimeofday(&start, NULL);
			printf("algorithem pro: %x\n",gDmsAlgorithemImgList[index]);
			lc_dvr_bridge_dms_test((gDmsAlgorithemImgList[index]), 0, &result); // (gDmsAlgorithemImgList[index])			
			if(result!=0)
			{
				lc_dvr_algorithem_notify(result);
			}
			gettimeofday(&end, NULL);
			temp = start.tv_sec*1000 + (start.tv_usec/1000);
			interval = end.tv_sec*1000 + (end.tv_usec/1000);
			interval -= temp;
			printf("Algorithem interval dms = %d ms\n", interval);
			gDmsImgIndexHead++;
		}

		
		if(gAdasImgIndexTail!=gAdasImgIndexHead && Adas_Switch)
		{
			int index = gAdasImgIndexHead&0x03;
			gettimeofday(&start, NULL);
			printf("algorithem pro: %x\n",gAdasAlgorithemImgList[index]);
			lc_dvr_bridge_adas_test((gAdasAlgorithemImgList[index]), 0, &result); // (gAdasAlgorithemImgList[index])			
			
			gettimeofday(&end, NULL);
			temp = start.tv_sec*1000 + (start.tv_usec/1000);
			interval = end.tv_sec*1000 + (end.tv_usec/1000);
			interval -= temp;
			printf("Algorithem interval adas = %d ms\n", interval);
			gAdasImgIndexHead++;
		}
		
	}
}
#endif

/**Stream用通道384x384，DMS跟ADAS算法使用的视频流**/
void *vc_channel4_handler(void *arg)
{
    int ret = -1;
	VIM_VPSS_FRAME_INFO_S stFrameInfo = {0};

	logd_lc("vc_channel4_handler\n");	
    while (VIM_FALSE == View_Exit)
    {
        ret = VIM_MPI_VPSS_GetFrame(0, LC_VC_384x384_CHANEL, &stFrameInfo, 3000);

        if(0 != ret)
        {
            logd_lc("CHL3 VPSS_GetFrame err %x\n", ret);
            usleep(1);
            continue;
        }
        else
        {							
			if(stFrameInfo.u32SrcVidChn == 0)
			{
//				printf("deal DMS task!!!\n");
#ifdef LC_HAVE_ALGORITHEM			
				if(gDmsImgIndexTail==gDmsImgIndexHead && Dms_Switch)
				{
					char* colorR, *colorG, *colorB;
					int index = gDmsImgIndexTail&0x03;
					colorR = (char*)stFrameInfo.pVirAddrStd[0];
					colorG = (char*)stFrameInfo.pVirAddrStd[1];
					colorB = (char*)stFrameInfo.pVirAddrStd[2];
					lc_rgb_convert(colorR, colorG, colorB, gDmsAlgorithemImgList[index], 384, 384);
					gDmsImgIndexTail++;		
					logd_lc("post frame dms %x", gDmsAlgorithemImgList[index]);
					sem_post(&semAlgorithem);
				}
#endif				
				mDmsFrameRate++;
			}
			else if(stFrameInfo.u32SrcVidChn == 1)
			{
//				printf("deal ADAS task!!!\n");
//				{}

#ifdef LC_HAVE_ALGORITHEM	
				if(gAdasImgIndexTail==gAdasImgIndexHead && Adas_Switch)
				{
					char* colorR, *colorG, *colorB;
					int index = gAdasImgIndexTail&0x03;
					colorR = (char*)stFrameInfo.pVirAddrStd[0];
					colorG = (char*)stFrameInfo.pVirAddrStd[1];
					colorB = (char*)stFrameInfo.pVirAddrStd[2];
					lc_rgb_convert(colorR, colorG, colorB, gAdasAlgorithemImgList[index], 384, 384);
					gAdasImgIndexTail++;		
					logd_lc("post frame adas %x", gAdasAlgorithemImgList[index]);
					sem_post(&semAlgorithem);
				}
#endif
				mAdasFrameRate++;

//					char* colorR, *colorG, *colorB;
//					int index = gAdasImgIndexTail&0x03;
//					colorR = (char*)stFrameInfo.pVirAddrStd[0];
//					colorG = (char*)stFrameInfo.pVirAddrStd[1];
//					colorB = (char*)stFrameInfo.pVirAddrStd[2];
//					lc_rgb_convert(colorR, colorG, colorB, gAdasAlgorithemImgList[index], 384, 384);
//#ifdef LC_HAVE_ALGORITHEM					
//					// lc_dvr_bridge_dms_test(NULL, 1);//(gAdasAlgorithemImgList[index]);
//					// sem_post(&semAlgorithem);
//#endif					
//					gAdasImgIndexTail++;
			}		
		}
			
		VIM_MPI_VPSS_GetFrame_Release(0, LC_VC_384x384_CHANEL, &stFrameInfo);
        usleep(1);
	}	
	
	logd_lc("vc_chanel4 Exit!\n");
    return NULL;
}


void release_vpssBuf()
{
	int vpssId = 0;
    int ret = 0;
    int group = 0;
	VIM_VPSS_FRAME_INFO_S releaseBuf = {0};
	voInterruptMsg voMsg;
	memset(&voMsg, 0, sizeof(voInterruptMsg));

	while(VIM_FALSE == View_Exit)
	{
        ret = VIM_MPI_VO_GetInterredMsg(&voMsg);
        if(ret)
        {
            break;
        }

        ret = getVpssFrameList(voMsg.interruptAddr, &releaseBuf, &group, &vpssId);
        if(0 == ret)
        {
            ret = VIM_MPI_VPSS_GetFrame_Release(group, vpssId, &releaseBuf);
            if(ret != VIM_SUCCESS)
            {
                printf("release VIM_MPI_VPSS_GetFrame_Release err %x\n",ret);
            }
        }
        else 
        {
            printf("release_vpssBuf error: not find frame: group:%d, chan:%d, addr:%#x\n", group, voMsg.chan, voMsg.interruptAddr);
        }
	}
}


void get_voInterrupt()
{
	int ret = 0;
	int fd  = open(DE_HD_PATH, O_RDWR, 0);
	struct timeval timeout = {0};
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	while(1)
	{
		if(VIM_TRUE == View_Exit)
		{
			close(fd);
			break;
		}
		ret = VIM_MPI_VO_SelectFd(fd,&timeout);
		if(-1 == ret )
		{
			printf("select err\n");
		}
		else if(0 == ret)
		{
			printf("VIM_MPI_VO_SelectFd timeout!!!\n");
		}
		else
		{
            release_vpssBuf();
		}
	}
}

VIM_S32 ToolsBind_DCVI_VPSS_Stop(void)
{
	VIM_S32 s32Ret = 0;
    VIM_U32 type=0;
    SYS_BD_CHN_S srcchn[4]={0}, dstchn={0};
    srcchn[0].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[0].s32DevId=0x1;
    srcchn[0].s32ChnId=0x0;

    srcchn[1].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[1].s32DevId=0x1;
    srcchn[1].s32ChnId=0x1;

    srcchn[2].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[2].s32DevId=0x1;
    srcchn[2].s32ChnId=0x2;

    srcchn[3].enModId=VIM_BD_MOD_ID_DCVI;
    srcchn[3].s32DevId=0x1;
    srcchn[3].s32ChnId=0x3;

    dstchn.enModId=VIM_BD_MOD_ID_VPSS;
    dstchn.s32DevId=VIM_VPSS_BIND_DRVID;
    dstchn.s32ChnId=VPSS_BIND_INPUT_0;
    type=VIM_BIND_WK_TYPE_DRV;
    s32Ret=VIM_MPI_SYS_UnBind(4,&srcchn[0], 1,&dstchn, type);
	if(s32Ret != 0)
		printf("DCVI VO VIM_MPI_SYS_UnBind fail.\n");
	printf("function: %s line:%d s32Ret:%d.\n", __func__, __LINE__,s32Ret);
	return s32Ret;
}
/******************************************************************************
 * function : Binding
 ******************************************************************************/
#define BIND_VPSS_CHN_NUM 4
VIM_S32 SAMPLE_VpssBind4Venc(VIM_U32 u32SrcDev, VIM_U32 u32SrcChn, VIM_U32 u32DstDev,VIM_U32 u32DstChn)
{
	VIM_S32 s32Ret = 0;

	SAMPLE_PRT(" start bind venc u32SrcDev:%d u32SrcChn:%d u32DstDev:%d u32DstChn:%d...\n",
		u32SrcDev, u32SrcChn, u32DstDev, u32DstChn);
	
	SYS_BD_CHN_S pstSrcChn = {0};
	SYS_BD_CHN_S pstDrcChn[4] = {0};

	pstSrcChn.enModId	= VIM_BD_MOD_ID_VPSS;
	pstSrcChn.s32ChnId	= u32SrcChn+3;
	pstSrcChn.s32DevId	= u32SrcDev;

	pstDrcChn[0].enModId	= VIM_BD_MOD_ID_VENC;
	pstDrcChn[0].s32ChnId	= u32DstChn;
	pstDrcChn[0].s32DevId	= u32DstDev;

	pstDrcChn[1].enModId	= VIM_BD_MOD_ID_VENC;
	pstDrcChn[1].s32ChnId	= u32DstChn+1;
	pstDrcChn[1].s32DevId	= u32DstDev;

	pstDrcChn[2].enModId	= VIM_BD_MOD_ID_VENC;
	pstDrcChn[2].s32ChnId	= u32DstChn+2;
	pstDrcChn[2].s32DevId	= u32DstDev;

	pstDrcChn[3].enModId	= VIM_BD_MOD_ID_VENC;
	pstDrcChn[3].s32ChnId	= u32DstChn+3;
	pstDrcChn[3].s32DevId	= u32DstDev;

	s32Ret = VIM_MPI_SYS_Bind(1,&pstSrcChn,BIND_VPSS_CHN_NUM,&pstDrcChn[0],VIM_BIND_WK_TYPE_USR);
	if(s32Ret != 0){
		SAMPLE_PRT("vpss bind venc VIM_MPI_SYS_Bind error  s32Ret=0x%x \n",s32Ret);
	}

	SAMPLE_PRT("vpss bind venc end\n");

	return s32Ret;
}

VIM_S32 SAMPLE_VpssUnBind4Venc(VIM_U32 u32SrcDev, VIM_U32 u32SrcChn, VIM_U32 u32DstDev,VIM_U32 u32DstChn)
{
	VIM_S32 s32Ret = 0;

	SAMPLE_PRT(" start vpss unbind venc u32SrcDev:%d u32SrcChn:%d u32DstDev:%d u32DstChn:%d...\n",
		u32SrcDev, u32SrcChn, u32DstDev, u32DstChn);
	
	SYS_BD_CHN_S pstSrcChn 		= {0};
	SYS_BD_CHN_S pstDrcChn[4] 	= {0};

	pstSrcChn.enModId	= VIM_BD_MOD_ID_VPSS;
	pstSrcChn.s32ChnId	= u32SrcChn+3;
	pstSrcChn.s32DevId	= u32SrcDev;

	pstDrcChn[0].enModId	= VIM_BD_MOD_ID_VENC;
	pstDrcChn[0].s32ChnId	= u32DstChn;
	pstDrcChn[0].s32DevId	= u32DstDev;

	pstDrcChn[1].enModId	= VIM_BD_MOD_ID_VENC;
	pstDrcChn[1].s32ChnId	= u32DstChn+1;
	pstDrcChn[1].s32DevId	= u32DstDev;

	pstDrcChn[2].enModId	= VIM_BD_MOD_ID_VENC;
	pstDrcChn[2].s32ChnId	= u32DstChn+2;
	pstDrcChn[2].s32DevId	= u32DstDev;

	pstDrcChn[3].enModId	= VIM_BD_MOD_ID_VENC;
	pstDrcChn[3].s32ChnId	= u32DstChn+3;
	pstDrcChn[3].s32DevId	= u32DstDev;

	s32Ret = VIM_MPI_SYS_UnBind(1,&pstSrcChn,BIND_VPSS_CHN_NUM,&pstDrcChn[0],VIM_BIND_WK_TYPE_USR);
	if(s32Ret != 0){
		SAMPLE_PRT("vpss bind venc VIM_MPI_SYS_Bind error  s32Ret=0x%x \n",s32Ret);
	}

	SAMPLE_PRT("vpss unbind venc end\n");

	return s32Ret;
}

static void sample_HandleSig(VIM_S32 signo)
{
    SAMPLE_PRT("signal %d\n", signo);

    if (SIGINT == signo || SIGTERM == signo || SIGQUIT == signo)
    {
		printf ("=====> %s %d\n",__func__,__LINE__);
        View_Exit = VIM_TRUE;
		SAMPLE_PRT("%s exit\n", __FUNCTION__);
        fclose(stdin);

        exit(0);
    }
}

VIM_S32 SAMPLE_VENC_TEST(SAMPLE_ENC_CFG *venc_cfg_config)
{
    VIM_S32 s32Ret;
    VIM_S8 threadname[16];

    memset(threadname, 0x0, 16);
    sprintf((char *) threadname, "test_Venc_chn%d", venc_cfg_config->VeChn);
    prctl(PR_SET_NAME, threadname);

    s32Ret = SAMPLE_COMM_VENC_INIT(venc_cfg_config);
    if (VIM_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VENC_INIT [%d] faild with %#x! ===\n", venc_cfg_config->VeChn, s32Ret);
        View_Exit = VIM_TRUE;
        return VIM_FAILURE;
    }

    SAMPLE_PRT("%s VeChn:%d enter\n", __func__, venc_cfg_config->VeChn);
    venc_cfg_config->stopwork_flag = 0;
    venc_cfg_config->dowork_flag = 0;
    s32Ret = SAMPLE_COMM_VENC_OPEN(venc_cfg_config);
    if (VIM_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VENC_OPEN [%d] faild with %#x! ===\n", venc_cfg_config->VeChn, s32Ret);
        View_Exit = VIM_TRUE;
        return VIM_FAILURE;
    }
	venc_cfg_config->u8IsUseNonVencFrameBuf = 1;
	venc_cfg_config->u8SelectMode = 1;
	venc_cfg_config->u8SelectModeEnable = 1;
	SAMPLE_SET_VENV_BufInfo(venc_cfg_config);
    
    s32Ret = SAMPLE_COMM_VENC_START(venc_cfg_config);
    if (VIM_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VENC_START [%d] faild with %#x! ===\n", venc_cfg_config->VeChn, s32Ret);
        View_Exit = VIM_TRUE;
        return VIM_FAILURE;
    }

    SAMPLE_COMM_VENC_StartGetStream(venc_cfg_config->VeChn, 1, 0, venc_cfg_config);
    

    venc_cfg_config->dowork_flag = 1;
    while (venc_cfg_config->dowork_flag && !View_Exit) {
        usleep(600 * 1000);
    }

    SAMPLE_COMM_VENC_StopGetStream(venc_cfg_config->VeChn, venc_cfg_config);

    s32Ret = SAMPLE_COMM_VENC_STOP(venc_cfg_config);
    if (VIM_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VENC_CLOSE [%d] faild with %#x! ===\n", venc_cfg_config->VeChn, s32Ret);
    }

    s32Ret = SAMPLE_COMM_VENC_CLOSE(venc_cfg_config);
    if (VIM_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VENC_CLOSE [%d] faild with %#x! ===\n", venc_cfg_config->VeChn, s32Ret);
    }

    venc_cfg_config->stopwork_flag = 1;
    SAMPLE_PRT("%s VeChn:%d exit\n", __func__, venc_cfg_config->VeChn);

    s32Ret = SAMPLE_COMM_VENC_EXIT(venc_cfg_config);
    if (VIM_SUCCESS != s32Ret) {
        SAMPLE_PRT(" SAMPLE_COMM_VENC_EXIT failed 0x%x\n", s32Ret);
    }

    return VIM_SUCCESS;
}

#define CHAN_MAX 4
VIM_S32 TestView(sample_config *config)
{

	VIM_S32 s32Ret = 0, i = 0 ;
	vo_options voCfg = {0};
	dcvi_attr_str dcvi_attr = {0};
	pthread_t feed_thread[6];
	VIM_CHN_CFG_ATTR_S stChnAttr 		= {0};
	VIM_VPSS_GROUP_ATTR_S vpssGrpAttr 	= {0};
	
	VIM_U32 u32ChnId = 0;
	VIM_U32 u32GrpID = 0;
	pthread_t vo_get_interrupt = -1;

	s32Ret = SAMPLE_DCVI_GetAttrFromIni(config->dcvi_config.name[0],&dcvi_attr);
	if(s32Ret != VIM_SUCCESS){
		printf("SAMPLE_DCVI_GetAttrFromIni error:%d \r\n",s32Ret);
		return -1;
	}

	if (strlen(config->vo_config.name[0]) <= 0) {
        SAMPLE_PRT("disable vo\n");
    } else {
		s32Ret = vo_readParam(&voCfg,config->vo_config.name[0]);
		if(s32Ret != VIM_SUCCESS){
			printf("vo_readParam error:%d \r\n",s32Ret);
			return -2;
		}
    }
	/******************************************
	 vpss module start
	******************************************/
	s32Ret = PARAM_GetManCtrlInfoFromIni(config->vpss_config.name[0], 0xff, 0xff);
	if(s32Ret < 0){
		goto step1;
	}

	for(u32GrpID = 0; u32GrpID<16; u32GrpID++){
		if(! ((mstManInfo.u32GrpMask>>u32GrpID) & 0x1))
			continue;
		
		memcpy(&grpAttr[u32GrpID].stGrpInput, &chnInAttr[u32GrpID], sizeof(VIM_VPSS_INPUT_ATTR_S));
		memcpy(&vpssGrpAttr, &grpAttr[u32GrpID], sizeof(VIM_VPSS_GROUP_ATTR_S));
		

		printf("u32GrpID: %d w:%d h:%d.\n",u32GrpID,vpssGrpAttr.stGrpInput.u16Width, vpssGrpAttr.stGrpInput.u16Height);
		s32Ret = SAMPLE_COMMON_VPSS_StartGroup(u32GrpID, &vpssGrpAttr);
		if(s32Ret != VIM_SUCCESS)
			goto step2;

		for(u32ChnId = 0; u32ChnId<MAX_VPSS_CHN; u32ChnId++){
			if(! ((mstManInfo.u32ChnOutMask[u32GrpID]>>u32ChnId) & 0x1))
				continue;			
			/*
			*将ini配置文件的Group Channel参数全部复制到stChnAttr中
			*/
			memcpy(&stChnAttr, &chnOutAttr[u32GrpID][u32ChnId], sizeof(VIM_CHN_CFG_ATTR_S));

			printf("vpss config u32GrpID[%d]:%d chn:%d.. \n",__LINE__, u32GrpID, u32ChnId);
			// logd_lc("OutPut[%d] enFormat=%d\n", u32ChnId, stChnAttr.enPixfmt);
			//====================================================================
			if(mstManInfo.u32DstMod[u32GrpID][u32ChnId] == VIM_ID_VENC)
			{
				logd_lc("video chanel cfg!");
				printf("u16Width->%d\n", chnOutAttr[u32GrpID][u32ChnId].u16Width);
				printf("u16Height->%d\n", chnOutAttr[u32GrpID][u32ChnId].u16Height);
				printf("u32Denominator->%d\n", chnOutAttr[u32GrpID][u32ChnId].u32Denominator);
				chnOutAttr[u32GrpID][u32ChnId].u16Width	= LC_RECORD_VIDEO_NSIZE_W;
				chnOutAttr[u32GrpID][u32ChnId].u16Height = LC_RECORD_VIDEO_NSIZE_H;
			}
			//====================================================================
			s32Ret = VIM_MPI_VPSS_SetChnOutAttr(u32GrpID, u32ChnId, &stChnAttr);
            if(s32Ret != VIM_SUCCESS)
				goto step2;

#ifdef OSD_ENABLE
            /* show osd on vo */
            if ((mstManInfo.u32OsdOpen & VIM_BIT0)&&(u32ChnId != LC_VC_384x384_CHANEL))
            {
                VIM_CHAR sTStr[128] = {0};
                sprintf(sTStr, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c", 0xd6, 0xd0, 0xd0, 0xc7, 0xce, 0xa2, 
                        0x41, 0x49, 0xD0, 0xBE, 0xC6, 0xAC, 0x2d,0x2d, 0xcd, 0xa8, 0xb5, 0xc0); /*GB2312编码*/

                VIM_MPI_RGN_Init(u32GrpID);
                dvr_DO_REGION_OSD_Init(u32GrpID, u32ChnId, stChnAttr.u16Width, stChnAttr.u16Height, sTStr, 1);
            }
#endif

            if(mstManInfo.u32DstMod[u32GrpID][u32ChnId] != VIM_ID_VENC)
            {
                s32Ret = SAMPLE_COMMON_VPSS_StartChn(u32GrpID, u32ChnId, &stChnAttr);
                if(s32Ret != VIM_SUCCESS)
                    goto step2;
            }
        }
    }

    /******************************************
        Venc module start
     ******************************************/
#if 0
	VIM_U8 cfgfilenum = 1;
    VIM_U8 cfgchnnum = 0;
    VIM_U8 chnnum = 0;
    cfgfilenum = config->venc_config.num;
	for(i = 0;i < cfgfilenum;i++){
		cfgfilename[i] = config->venc_config.name[i];
		s32Ret = SAMPLE_COMM_VENC_PARACFG(cfgfilename[i],&pEncCfg[chnnum],&cfgchnnum);
		if (VIM_SUCCESS != s32Ret){
			SAMPLE_PRT(" parase %s faild with %#x! ===\n",cfgfilename[i], s32Ret);
			//goto venc_exit;
		}
		chnnum += cfgchnnum;
	}
	SAMPLE_PRT( "cfgnum = %d chnnum=%d\n",cfgfilenum,chnnum);
	for(i=0;i<chnnum;i++){
		pEncCfg[i].chnnum = chnnum;
        s32Ret = pthread_create(&thread_id[i], NULL, (VIM_VOID*)SAMPLE_VENC_TEST,&pEncCfg[i]);
		SAMPLE_PRT(" pthread_create s32Ret=0x%x\n",s32Ret);
		while(View_Exit == VIM_FALSE){
			if((pEncCfg[i].dowork_flag ==1)&&(pEncCfg[i].venc_bind ==1)){//2:bind vpss
				break;
			}
			usleep(1000);
		}
		SAMPLE_PRT(" pthread_create test VeChn=%d s32Ret=0x%x\n",pEncCfg[i].VeChn,s32Ret);
    }
	if(chnnum>0){
		SAMPLE_VpssBind4Venc(0, 4, 0, 0);
	}
#else
#ifdef RECORD_DEBUG
	int vencCh = 0;
	for(;vencCh<4;)
	{
		int ret = VimVencChStart(vencCh, recInfo.vFmt, &recInfo.vInfo, recInfo.bitRate, BIND_VPSS_TYPE, NULL, 0);
		if(ret != 0)
		{
			printf("error:VimVencChStart ch:%d ret:%d\n", vencCh, ret);
			goto step1;
		}
		vencCh++;
	}

	SAMPLE_VpssBind4Venc(0, 1, 0, 0);
#endif
#endif

	//venc after
	for(u32GrpID = 0; u32GrpID<16; u32GrpID++)
	{
		if(0 != ((mstManInfo.u32GrpMask>>u32GrpID) & 0x1))
			break;;
	}
	
	for(u32ChnId = 0; u32ChnId<MAX_VPSS_CHN; u32ChnId++)
	{
		if(! ((mstManInfo.u32ChnOutMask[u32GrpID]>>u32ChnId) & 0x1))
			continue;
		if(mstManInfo.u32DstMod[u32GrpID][u32ChnId] == VIM_ID_VENC)
		{
			SAMPLE_PRT(" vpss bind venc chn:%d! \n",u32ChnId);
		}
		else
		{
			continue;
		}
		SAMPLE_PRT("u32GrpID:%d u32ChnId:%d..\n",u32GrpID, u32ChnId);

		s32Ret = VIM_MPI_VPSS_GetChnOutAttr(u32GrpID, u32ChnId, &stChnAttr);
		if(s32Ret != VIM_SUCCESS)
			 return -1;

		s32Ret = SAMPLE_COMMON_VPSS_StartChn(u32GrpID, u32ChnId, &stChnAttr);
		if(s32Ret != VIM_SUCCESS)
			 return -1;
	}
	
	/******************************************
        Venc module end
     ******************************************/

	printf("use GrpID: %d\n",u32GrpID);
	u32GrpID = 0;
	/******************************************
	 vo module start
	******************************************/
	int useVpssChan = 0;
	if (strlen(config->vo_config.name[0]) <= 0) 
	{
		SAMPLE_PRT("disable vo\n");
	} 
	else 
	{
		voCfg.binderType = NOBINDER;
		s32Ret = VO_Init_Enter(voCfg);
		int *vpssThread = (int*)sys_msg_get_memcache(64);
		
		if(s32Ret != VIM_SUCCESS)
		{
			printf("VO_Init_Enter error:%d \r\n",s32Ret);
			return -3;
		}
		logd_lc("u32ChnOutMask-->%x", mstManInfo.u32ChnOutMask[u32GrpID]);
		useVpssChan = LC_VC_1920x1080_CHANEL;
		if(((mstManInfo.u32ChnOutMask[u32GrpID]>>useVpssChan) & 0x1))
		{
			vpssThread[0] = useVpssChan;
			s32Ret = pthread_create(&feed_thread[0],NULL,(void*)sample_feed_vo,(void*)&vpssThread[0]);
			if(0 == s32Ret )
			{
				printf("feed_vo pthread%d SUCCESS\n", useVpssChan);
				pthread_detach(feed_thread[0]);
			}
			sleep(1);
		}
		
		useVpssChan = LC_VC_960X544_CHANEL;
		if(((mstManInfo.u32ChnOutMask[u32GrpID]>>useVpssChan) & 0x1))
		{
			vpssThread[1] = useVpssChan;
			s32Ret = pthread_create(&feed_thread[1],NULL,(void*)sample_feed_vo,(void*)&vpssThread[1]);
			if(0 == s32Ret )
			{
				printf("feed_vo pthread%d SUCCESS\n", useVpssChan);
				pthread_detach(feed_thread[1]);
			}
		}
		
		useVpssChan = LC_VC_1280X720_CHANEL;
		if(((mstManInfo.u32ChnOutMask[u32GrpID]>>useVpssChan) & 0x1))
		{
			vpssThread[2] = useVpssChan;
			s32Ret = pthread_create(&feed_thread[2],NULL,(void*)vc_channel3_handler,(void*)&vpssThread[2]);
			if(0 == s32Ret )
			{
				printf("feed_vo pthread%d SUCCESS\n", useVpssChan);
				pthread_detach(feed_thread[2]);
			}
		}

		useVpssChan = LC_VC_384x384_CHANEL;
		if(((mstManInfo.u32ChnOutMask[u32GrpID]>>useVpssChan) & 0x1))
		{
			vpssThread[3] = useVpssChan;
			s32Ret = pthread_create(&feed_thread[3],NULL,(void*)vc_channel4_handler,(void*)&vpssThread[3]);
			if(0 == s32Ret )
			{
				printf("feed_vo pthread%d SUCCESS\n", useVpssChan);
				pthread_detach(feed_thread[3]);
			}
#ifdef LC_HAVE_ALGORITHEM			
			s32Ret = pthread_create(&feed_thread[4],NULL,(void*)vc_algorithem_handler, NULL);
			if(0 == s32Ret )
			{
				printf("algorithem SUCCESS\n");
				pthread_detach(feed_thread[4]);
			}	
#endif			
		}

        pthread_create(&vo_get_interrupt,NULL,(void*)get_voInterrupt,NULL);
        pthread_detach(vo_get_interrupt);
	}

	s32Ret = ToolsBind_DCVI_VPSS_Start();
	if(s32Ret != VIM_SUCCESS){
		printf("ToolsBind_DCVI_VPSS_Start error:%d \r\n",s32Ret);
		goto step1;
	}

	/******************************************
	 dcvi module start
	******************************************/

	for(i = 0; i < DCVI_DEV_MAX; i++){
		if((dcvi_attr.u8ChnEn & 0x3<<i*2) == 0)
			continue;
		printf("%s:%d dev = %d \r\n",__func__,__LINE__,i);
		s32Ret = VIM_MPI_DCVI_SetDevAttr(i,&dcvi_attr.dcvi_dev[i]);
		ASSERT_RET_GOTO(s32Ret,step2);

		s32Ret = VIM_MPI_DCVI_EnableDev(i);
		ASSERT_RET_GOTO(s32Ret,step2);
	}
	for(i = 0; i < DCVI_CHN_MAX; i++){
		if((dcvi_attr.u8ChnEn & 1<<i) == 0)
			continue;
		printf("%s:%d chn = %d \r\n",__func__,__LINE__,i);
		s32Ret = VIM_MPI_DCVI_SetChnAttr(i,&dcvi_attr.dcvi_chn[i]);
		ASSERT_RET_GOTO(s32Ret,step2);
	}
	for(i = 0; i < DCVI_CHN_MAX; i++){
		if((dcvi_attr.u8ChnEn & 1<<i) == 0)
			continue;
		s32Ret = VIM_MPI_DCVI_EnableChn(i);
		ASSERT_RET_GOTO(s32Ret,step2);
	}	

#if 0
	printf(" View_Exit:%d... \n",View_Exit);
	SAMPLE_VIEW_Handle_Menu( view_menus);
	printf("[%s %d]\n",__FUNCTION__,__LINE__);
#else
    return s32Ret;
#endif
	
	/******************************************
	 unregister
	******************************************/
#if 0
	if(chnnum>0){
		SAMPLE_VpssUnBind4Venc(0, 4, 0, 0);
	}
    for (i = 0; i < chnnum; i++) {
        if (thread_id[i] != 0) {
            pthread_join(thread_id[i], 0);
        }
    }
#else
	SAMPLE_VpssUnBind4Venc(0, 1, 0, 0);
	for(int recCh=0;recCh<4;recCh++)
	{
		VimRecordChStop(recCh);
	}
#endif
    for (i = 0; i < 16; i++) 
	{
	    if (!((mstManInfo.u32GrpMask >> i) & 0x1)) 
		{
	        continue;
	    }
		printf("i %d\n",i);
	    SAMPLE_COMMON_VPSS_StopGroup(i);
	}
    SAMPLE_PRT("\n");	
step2:
#if 0
	s32Ret = ToolsBind_DCVI_VO_Stop();
	ASSERT_RET_PRT(s32Ret);	
#endif

	for(i = 0; i< DCVI_CHN_MAX; i++){
		if((dcvi_attr.u8ChnEn & 1<<i) == 0)
			continue;
		s32Ret = VIM_MPI_DCVI_DisableChn(i);
		ASSERT_RET_PRT(s32Ret);		
	}
	for(i = 0; i < DCVI_DEV_MAX; i++){
		if((dcvi_attr.u8ChnEn & 0x3<<i*2) == 0)
			continue;
		s32Ret = VIM_MPI_DCVI_DisableDev(i);
		ASSERT_RET_PRT(s32Ret);			
	}
step1:
	if (strlen(config->vo_config.name[0]) > 0) {
		printf("SAMPLE_VO_Cleanup \n");
		SAMPLE_VO_Cleanup();
	}

	SAMPLE_PRT("\n");
	return s32Ret;
}

#define PLAYBACK_GROUP_ID     (1)
#define PLAYBACK_CHAN_ID      (4)
#define PLAYBACK_VPSS_INPUT_CHAN    (E_VPSS_INPUT_1)

VIM_S32 vc_vpss_group1_init(void)
{
    /*1. 使能VPSS模块, , 设置VPSS GROUP组input通道参数*/
    VIM_S32 s32Ret = VIM_MPI_VPSS_EnableDev();
    ASSERT_RET_RETURN(s32Ret);

    /*2. 创建VPSS GROUP, 初始化VPSS GROUP组参数, 初始化VPSS GROUP组输入通道属性*/
    VIM_VPSS_GROUP_ATTR_S vpssGrpAttr = {0};
    vpssGrpAttr.u32GrpId                        = PLAYBACK_GROUP_ID;  /*VPSS GROUP序列号*/
    vpssGrpAttr.u32GrpAutoStart                 = VIM_FALSE;
    vpssGrpAttr.u32GrpStatus                    = E_VPSS_GROUP_STATIC;
    vpssGrpAttr.u32GrpDataSrc                   = PLAYBACK_VPSS_INPUT_CHAN;       /*重要参数: 对应3种输入方式VPSS_INPUT_0/1/2*/
    vpssGrpAttr.enInputType                     = E_INPUT_TYPE_MEM;
    vpssGrpAttr.u32ViIsVpssBuf                  = VIM_FALSE;
    vpssGrpAttr.stGrpInput.u16Width             = 1920;
    vpssGrpAttr.stGrpInput.u16Height            = 1080;
    vpssGrpAttr.stGrpInput.bMirror              = VIM_FALSE;
    vpssGrpAttr.stGrpInput.enCrop               = VIM_FALSE;
    vpssGrpAttr.stGrpInput.u16CropLeft          = 0;
    vpssGrpAttr.stGrpInput.u16CropTop           = 0;
    vpssGrpAttr.stGrpInput.u16CropWidth         = 0;
    vpssGrpAttr.stGrpInput.u16CropHeight        = 0;
    vpssGrpAttr.stGrpInput.enPixfmt             = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    vpssGrpAttr.stGrpInput.u16BufferNum         = 8;
    vpssGrpAttr.stGrpInput.u32WidthStride       = 1920;
    vpssGrpAttr.stGrpInput.u32HeightStride      = 1080;

    s32Ret = VIM_MPI_VPSS_CreatGrp(&vpssGrpAttr);
    ASSERT_RET_RETURN(s32Ret);

    /*3. 设置VPSS GROUP组输入通道属性*/
    s32Ret = VIM_MPI_VPSS_SetGrpInAttr(vpssGrpAttr.u32GrpId, &vpssGrpAttr);
    ASSERT_RET_RETURN(s32Ret);


    /*4. 设置VPSS GROUP输出通道参数*/
    VIM_CHN_CFG_ATTR_S chnOutAttr = {0};
    chnOutAttr.u16Width         = 1920;
    chnOutAttr.u16Height        = 1080;
    chnOutAttr.enPixfmt         = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    chnOutAttr.enYcbcr          = 0;        /*E_INPUT_YCBCR_DISABLE*/
    chnOutAttr.enCframe         = VIM_FALSE;
    chnOutAttr.u32Numerator     = 30;
    chnOutAttr.u32Denominator   = 30;
    chnOutAttr.u16BufferNum     = 8;
    chnOutAttr.u32VoIsVpssBuf   = VIM_TRUE;
    chnOutAttr.enCrop           = VIM_FALSE;
    chnOutAttr.u16CropLeft      = 0;
    chnOutAttr.u16CropTop       = 0;
    chnOutAttr.u16CropWidth     = 0;
    chnOutAttr.u16CropHeight    = 0;

    s32Ret = VIM_MPI_VPSS_SetChnOutAttr(PLAYBACK_GROUP_ID, PLAYBACK_CHAN_ID, &chnOutAttr);
    ASSERT_RET_RETURN(s32Ret);

    printf("vpss config u32GrpID:%d chn:%d.. \n", PLAYBACK_GROUP_ID, PLAYBACK_CHAN_ID);
    return s32Ret;
}

VIM_S32 vc_vpss_start_chn(void)
{
    VIM_S32 ret = VIM_FAILURE;

    /*5. 使能通道*/
    ret = VIM_MPI_VPSS_EnableChn(PLAYBACK_GROUP_ID, PLAYBACK_CHAN_ID);
    if (VIM_SUCCESS != ret)
    {
        printf("Failed to VIM_MPI_VPSS_EnableChn : %#x.\n", ret);
        return ret;
    }

    /*6. 启动通道*/
    ret = VIM_MPI_VPSS_StartChn(PLAYBACK_GROUP_ID, PLAYBACK_CHAN_ID);
    if (VIM_SUCCESS != ret)
    {
        printf("Failed to VIM_MPI_VPSS_StartChn : %#x.\n", ret);
        return ret;
    }

    return ret;
}


VIM_S32 vc_vpss_group1_exit(void)
{
    VIM_S32 ret = VIM_FAILURE;

    /*如果修改 GROUP 属性参数，必须先 GET 出来再修改*/
    VIM_VPSS_GROUP_ATTR_S vpssGrpAttr = {0};
    ret = VIM_MPI_VPSS_GetGrpAttr(PLAYBACK_GROUP_ID, &vpssGrpAttr);
    ASSERT_RET_RETURN(ret);

    ret = VIM_MPI_VPSS_StopChn(PLAYBACK_GROUP_ID, PLAYBACK_CHAN_ID);
    ASSERT_RET_RETURN(ret);
    printf("vpss stop group %d chn:%d ok.\n", PLAYBACK_GROUP_ID, PLAYBACK_CHAN_ID);

    ret = VIM_MPI_VPSS_DisableChn(PLAYBACK_GROUP_ID, PLAYBACK_CHAN_ID);
    ASSERT_RET_RETURN(ret);
    printf("vpss disable chn group %d chn:%d ok.\n", PLAYBACK_GROUP_ID, PLAYBACK_CHAN_ID);

    ret = VIM_MPI_VPSS_DestroyGrp(PLAYBACK_GROUP_ID);
    printf("vpss DestroyGrp group %d ok.\n", PLAYBACK_GROUP_ID);
    ASSERT_RET_RETURN(ret);

    /*设备不关闭, 录像还需要使用vpss*/
//    VIM_MPI_VPSS_DisableDev();
//    CHECK_VPSS_RET_RETURN(ret);
//    printf("vpss DisableDev ok.\n");

    return ret;
}


/*每秒钟上报一次*/
int OnPlayerStatus(int handle, int event, void *data, void *userData)
{
    if (event == PLAY_PROG_EVNET)
    {
        double progress = *(double *)data;
        printf("OnPlayerStatus:handle:%d,play progress:%f.\n", handle, progress);
    }
    else if(event == PLAY_OVER_EVENT)
    {
        play_Exit = VIM_TRUE;
        printf("OnPlayerStatus:handle:%d,play over\n", handle);
    }
    else if(event == PLAY_ERROR_EVENT)
    {
        play_Exit = VIM_TRUE;
        printf("OnPlayerStatus:handle:%d,play error\n", handle);
    }
    else if (event == PLAY_START_EVNET)
    {
        printf("OnPlayerStatus:handle:%d,play start event\n", handle);
        playStartEvent = 1;
    }

    return 0;
}


void *vc_player_handler(void *arg)
{
    printf("vc_player_handler() enter...\n");
    int ret = -1;
    int cndIdx = 0;
    int isRelease = 0;
    int releaseIdx = 0;
    VIM_VPSS_FRAME_INFO_S stFrameInfo = {0};


    voImageCfg setStride = {0};
    setStride.chan = DCVI_OUTPUT_CHN0;
    setStride.weight = 1920;
    setStride.voDev = VO_DEV_HD;
    setStride.LayerId = Layer_A;
//    usleep(5*33*1000);      /*不延迟的话, 在预览切换到回放时通道1会短暂出现串屏*/
    VIM_MPI_VO_SetImageSize(&setStride, VO_DEV_HD);

    VIM_MPI_VO_SetMultiMode(Multi_None, MULTI_CONTROL_AUTO, NULL, NULL);
    printf("change MultiMode Multi_None.\n");


#if 1
    MultiChanBind Vo_MultiChanInfo = {0};
    Vo_MultiChanInfo.width = 1920;
    Vo_MultiChanInfo.high = 1080;
    Vo_MultiChanInfo.chan = 0;      /*表示从VO模块0通道输出*/
    Vo_MultiChanInfo.workMode = Multi_None;
#else
    mainPreview = 5;
    showChan = recv_msg.info.zoom_msg.channel;
    isSetSize = 0;
#endif

    while ((VIM_FALSE == play_Exit) && (VIM_FALSE == View_Exit))
    {
        /*从VPSS第0组第0个通道获取DCVI通道图像数据*/
        ret = VIM_MPI_VPSS_GetFrame(1, 4, &stFrameInfo, 3000);
        if(0 != ret)
        {
            printf("VIM_MPI_VPSS_GetFrame[%d] err %x\n", __LINE__, ret);
            usleep(1);
            continue;
        }
        else
        {
            /*记录当前帧数据属于哪个DCVI通道*/
            Vo_MultiChanInfo.codeAddr_y = (char *)stFrameInfo.pPhyAddrStd[0];
            Vo_MultiChanInfo.codeAddr_uv = (char *)stFrameInfo.pPhyAddrStd[1];
        }

        if ((NULL == Vo_MultiChanInfo.codeAddr_y) || (NULL == Vo_MultiChanInfo.codeAddr_uv))
        {
            printf("FRAME NULL \n");
        }
        else
        {
            int bRelease = 1;
            pthread_rwlock_rdlock(&g_vo_rwlock);
            setVpssFrameList(&stFrameInfo, 1, 4);
			logd_lc("Decode %d\n", mDecodeFrameCnt++);
            ret = VIM_MPI_VO_SetFrameA(&Vo_MultiChanInfo, 1);
            if(ret == VIM_SUCCESS)
            {
                bRelease = 0;
            }
            else
            {
                getVpssFrameList(Vo_MultiChanInfo.codeAddr_y, NULL, NULL, NULL);
            }
            pthread_rwlock_unlock(&g_vo_rwlock);
            if(bRelease)
            {
                VIM_MPI_VPSS_GetFrame_Release(1, 4, &stFrameInfo);
            }
         }

        usleep(1);
    }

    /*退出后要还原到开始时的分块模式*/
    printf("multimode  = %d.\n", mainPreview);
    if (0 == mainPreview)
    {
        VIM_MPI_VO_SetMultiMode(Multi_2X2, MULTI_CONTROL_AUTO, NULL, NULL);
        setStride.weight = 960;
        VIM_MPI_VO_SetImageSize(&setStride, VO_DEV_HD);
    }
    else
    {
        VIM_MPI_VO_SetMultiMode(Multi_None, MULTI_CONTROL_AUTO, NULL, NULL);
    }
    isSetSize = 0;
    printf("play thread quit.\n");
    return NULL;
}

int GetRecFullPath(int ch, int event, char *fullpath)
{
    char date_tag[12] = {0};
    char time_tag[12] = {0};
    char file_path[128] = {0};

    time_t dwCurTime;
    dwCurTime = time(NULL);

    struct tm *pTime;
    pTime = localtime(&dwCurTime);
    strftime(date_tag, 12, "%F", pTime);
#ifdef PACK_H265
    strftime(time_tag, 12, "%H%M%S.mp4", pTime);
#else
    strftime(time_tag, 12, "%H%M%S.svc", pTime);
#endif

    if (ch < 0)
    {
        printf("error, invalid val, chan = %d.\n", ch);
        return -1;
    }

    if (0x10 == event)  /*紧急录像路径*/
    {
        snprintf(file_path, 128, "%s/alarm_ch%d_", ALARM_DIR_PATH, ch + 1);
        strcat(file_path, date_tag);
        strcat(file_path, "_");
        strcat(file_path, time_tag);

        strncpy(fullpath, file_path, 128);
        printf("alarm_video_fullpath = %s.\n", fullpath);
        return 0;
    }
    else
    {
        snprintf(file_path, 128, "%s%d/", CHANNEL_DIR_PATH, ch + 1);
        strcat(file_path, date_tag);
    }

    if (0 != access(file_path, F_OK))
    {
        if (0 != access(RECORD_DIR_PATH, F_OK))
            mkdir(RECORD_DIR_PATH, 0777);

        if (0 != access(VIDEO_DIR_PATH, F_OK))
            mkdir(VIDEO_DIR_PATH, 0777);

        char chan_path[64] = {0};
        snprintf(chan_path, 64, "%s%d", CHANNEL_DIR_PATH, ch + 1);
        if (0 != access(chan_path, F_OK))
            mkdir(chan_path, 0777);

        mkdir(file_path, 0777);
		//---------------------add for take picture---------------------
#ifdef VIM_WITH_PICTURE		
		sprintf(chan_path,"/mnt/usb/usb1/picture%d", ch);
        if (0 != access(chan_path, F_OK))
            mkdir(chan_path, 0777);
		printf("chan_path thumb = %s.\n", chan_path);
#endif		
		//--------------------------------------------------------------
    }

    strcat(file_path, "/");
    strcat(file_path, time_tag);
    strncpy(fullpath, file_path, 128);

    printf("fullpath = %s.\n", fullpath);
	//------------------------------------
#ifdef VIM_WITH_PICTURE	
{
	char imagePath[128];
	sprintf(imagePath,"/mnt/usb/usb1/picture%d/%s", ch, time_tag);
	vc_jpeg_take_picture(ch, imagePath);
}
#endif
	//------------------------------------
    return 0;
}


long long comm_getDiskFreeSpace(const char *path)
{
    long long freeSpace = 0;
    struct statfs diskStatfs;
    if (statfs(path, &diskStatfs) >= 0)
    {
        /*空闲块数*每块字节数/(1024*1024)=剩余空间MB*/
        freeSpace = (((long long)diskStatfs.f_bsize * (long long)diskStatfs.f_bfree) / (long long)1048576);
    }

    printf("freeSpace %lld MB\n", freeSpace);
    return freeSpace;
}

/*找出并删除一个最早的录像文件*/
int delOldRecordFile()
{
#define PATH_MAX        (512)
#define BUFF_SIZE       (32)

    int i = 0;
    DIR *ptr_DIR = NULL;
    struct dirent *ptr_dirent = NULL;
    struct stat stat_info;
    char recordDirPath[PATH_MAX] = {0};
    char oldDirPath[PATH_MAX] = {0};
    char oldFilePath[PATH_MAX] = {0};
    char delFilePath[4][PATH_MAX] = {0};
    char fullDirPath[PATH_MAX] = {0};

    int ret = -1;
    int num = 0;
    int count = 0;
    char tempBuf[BUFF_SIZE] = {0};
    char fileCreateTime[BUFF_SIZE] = {0};

    for (i = 0; i < 4; i++)
    {
        snprintf(recordDirPath, PATH_MAX, "%s%d", CHANNEL_DIR_PATH, i + 1);
//        printf("LINE = %d, recordDirPath = %s, i = %d\n", __LINE__, recordDirPath, i);

        /*找到最早的日期文件夹*/
        if ((ptr_DIR = opendir(recordDirPath)) != NULL)
        {
            count = 0;
            memset(oldDirPath, '\0', sizeof(oldDirPath));

            while ((ptr_dirent = readdir(ptr_DIR)) != NULL)
            {
//                printf("LINE = %d, ptr_dirent->d_name = %s\n", __LINE__, ptr_dirent->d_name);
                if (strcmp(ptr_dirent->d_name, ".") == 0 || strcmp(ptr_dirent->d_name, "..") == 0)
                    continue;

                memset(fullDirPath, 0, sizeof(fullDirPath));
                snprintf(fullDirPath, PATH_MAX, "%s/%s", recordDirPath, ptr_dirent->d_name);
//                printf("LINE = %d, fullDirPath = %s\n", __LINE__, fullDirPath);

                if (lstat(fullDirPath, &stat_info) != 0)
                {
                    closedir(ptr_DIR);
                    printf("get path status failed, errno = %s.\n", strerror(errno));
                    /*是否需要删除*/
                    continue;
                }

                if (S_ISDIR(stat_info.st_mode))
                {
//                    printf("LINE = %d, fullDirPath = %s, oldDirPath = %s\n", __LINE__, fullDirPath, oldDirPath);
                    /*用字符串比较还是文件时间来判断?*/
                    if (0 == strlen(oldDirPath))
                    {
                        strncpy(oldDirPath, fullDirPath, PATH_MAX);
                    }
                    else
                    {
                        ret = strncmp(fullDirPath, oldDirPath, PATH_MAX);
                        if (ret < 0)
                        {
                            bzero(oldDirPath, PATH_MAX);
                            strncpy(oldDirPath, fullDirPath, PATH_MAX);
                            printf("LINE = %d, oldDirPath = %s\n", __LINE__, oldDirPath);
                        }
                    }
                }
                count++;
            }
            closedir(ptr_DIR);
            ptr_DIR = NULL;
            ptr_dirent = NULL;
        }
        else
        {
            printf("open dir failed, LINE = %d, dir  = %s, errno = %s.\n", __LINE__, recordDirPath, strerror(errno));
            continue;
        }

        /*某个通道没有录像日期文件夹*/
        if (count <= 0)
        {
            printf("not found folder, LINE = %d, count = %d, dir = %s is empty.\n", __LINE__, count, recordDirPath);
            continue;
        }

//        printf("LINE = %d, oldDirPath = %s\n", __LINE__, oldDirPath);
        /*找出最早的录像文件*/
        if ((ptr_DIR = opendir(oldDirPath)) != NULL)
        {
            count = 0;
            memset(oldFilePath, '\0', sizeof(oldFilePath));

            while ((ptr_dirent = readdir(ptr_DIR)) != NULL)
            {
//                printf("LINE = %d, ptr_dirent->d_name = %s\n", __LINE__, ptr_dirent->d_name);
                if (strcmp(ptr_dirent->d_name, ".") == 0 || strcmp(ptr_dirent->d_name, "..") == 0)
                    continue;

                memset(fullDirPath, 0, sizeof(fullDirPath));
                snprintf(fullDirPath, PATH_MAX, "%s/%s", oldDirPath, ptr_dirent->d_name);
//                printf("LINE = %d, fullDirPath = %s\n", __LINE__, fullDirPath);

                if (lstat(fullDirPath, &stat_info) != 0)
                {
                    closedir(ptr_DIR);
                    printf("get fromPath status failed, errno = %s.\n", strerror(errno));
                    continue;
                }

                if (S_ISREG(stat_info.st_mode))
                {
//                    printf("LINE = %d, fullDirPath = %s, oldFilePath = %s\n", __LINE__, fullDirPath, oldFilePath);
                    if (0 == strlen(oldFilePath))
                    {
                        strncpy(oldFilePath, fullDirPath, PATH_MAX);
                    }
                    else
                    {
                        ret = strncmp(fullDirPath, oldFilePath, PATH_MAX);
                        if (ret < 0)
                        {
                            bzero(oldFilePath, PATH_MAX);
                            strncpy(oldFilePath, fullDirPath, PATH_MAX);
//                            printf("LINE = %d, oldDirPath = %s\n", __LINE__, oldFilePath);
                        }
                    }
                }
                count++;
            }
            closedir(ptr_DIR);
            ptr_DIR = NULL;
            ptr_dirent = NULL;
        }
        else
        {
            printf("open dir failed, LINE = %d, errno = %s.\n", __LINE__, strerror(errno));
            continue;
        }

        /*需要判断找到的路径是否为空, 如果为空, 删除重新查找*/
        if (count <= 0)
        {
            printf("not found mp4 file, LINE = %d, count = %d, dir = %s is empty, delete...\n", __LINE__, count, oldDirPath);
            if (0 != rmdir(oldDirPath))
                printf("delete dirent failed, dir = %s, errno = %s.\n", oldDirPath, strerror(errno));
            i--;
            continue;
        }

        /*把搜索到的最早录像文件路径保存起来*/
        strncpy(&delFilePath[i][0], oldFilePath, PATH_MAX);
//        printf("LINE = %d, delFilePath[%d][0] = %s, oldDirPath = %s.\n", __LINE__, i, &delFilePath[i][0], oldFilePath);
    }

    /*在4个文件里再找出最早的录像文件*/
    int prefix_len = strlen(CHANNEL_DIR_PATH) + 2;
    for (i = 0, num = 0; i < 4; i++)
    {
        memset(tempBuf, 0, sizeof(tempBuf));
        memset(&stat_info, 0, sizeof(stat_info));

//        printf("LINE = %d, delFilePath[%d][0] = %s.\n", __LINE__, i, &delFilePath[i][0]);
        if (0 == strlen(&delFilePath[i][0]))
            continue;
        lstat(&delFilePath[i][0], &stat_info);

        if (S_ISREG(stat_info.st_mode))
        {
            /*按文件名(日期时间)来比较*/
            snprintf(tempBuf, BUFF_SIZE, "%s", &delFilePath[i][0] + prefix_len);

//            printf("LINE = %d, i = %d, fileCreateTime = %s, tempBuf = %s\n", __LINE__, i, fileCreateTime, tempBuf);
            if (0 == strlen(fileCreateTime))
            {
                strncpy(fileCreateTime, tempBuf, BUFF_SIZE);
                num = i;
            }
            else
            {
                ret = strncmp(tempBuf, fileCreateTime, BUFF_SIZE);
                if (ret < 0)
                {
                    strncpy(fileCreateTime, tempBuf, BUFF_SIZE);
                    num = i;
                    printf("LINE = %d, num = %d, fileCreateTime = %s\n", __LINE__, num, fileCreateTime);
                }
            }
        }
    }

    printf("LINE = %d, num = %d, fileCreateTime = %s, delFilePath = %s.\n", __LINE__, num, fileCreateTime, &delFilePath[num][0]);
    if ((num >= 0) && (num < 4))
    {
		//-------------------------------------------------
#ifdef VIM_WITH_PICTURE		
		char PicName[256];
		char *s8Ptr = &delFilePath[num][0];
		int i = 0;
		
		for(i=strlen(s8Ptr); i>0; --i)
		{
			if(s8Ptr[i]=='/')
			{
				break;
			}
		}
		
		sprintf(PicName,"/mnt/usb/usb1/picture%d/%s", num, s8Ptr+i+1);
		strchr(PicName, '.')[1] = '\0'; 
		strcat(PicName, EXT_PIC);
		
		if (0 != unlink(PicName))
		{
			printf("delete picture failed, dir = %s, errno = %s.\n", oldDirPath, strerror(errno));
		}
		else
		{
			printf("delete picture %s success!\n", PicName);
		}
#endif		
		//-------------------------------------------------    
        if (0 != unlink(&delFilePath[num][0]))
            printf("delete dirent failed, dir = %s, errno = %s.\n", oldDirPath, strerror(errno));
		else
			printf("delete video %s success!\n", &delFilePath[num][0]);
    }

    return 0;
}

/*所有有效通道开始录像*/
VIM_S32 vc_record_start(void)
{
    VIM_S32 ret = -1;
    unsigned char pos[4] = {4, 6, 0, 2};  /*0xef, 0xbf, 0xfe, 0xfb*/

    for (int i = 0; i < 4; i++)
    {
#ifdef RECORD_DEBUG
        if (((dcviChannelMask >> pos[i]) | 0xfe) == 0xfe)
        {
            ret = VimRecordChStart(i, i, &recInfo);
            if(ret != 0)
            {
                printf("error:VimRecordChStart ch:%d value:%#x ret:%d\n", i, dcviChannelMask >> pos[i], ret);
            }
        }
        else
        {
            ret = VimRecordChStop(i);
            if(ret != 0)
            {
                printf("error:VimRecordChStop ch:%d value:%#x ret:%d\n", i, dcviChannelMask >> pos[i], ret);
            }
        }
#endif
    }

    printf("reset all video channel, dcviChannelMask = %d.\n", dcviChannelMask);
    return ret;
}

/*所有通道停止录像*/
VIM_S32 vc_record_stop(void)
{
    VIM_S32 ret = -1;

    for (int i = 0; i < 4; i++)
    {
#ifdef RECORD_DEBUG
        ret = VimRecordChStop(i);
        if(ret != 0)
        {
            printf("error:VimRecordChStop ch:%d value:%#x ret:%d\n", i, dcviChannelMask, ret);
        }
#endif
    }

    printf("all dcvi channel stop record.\n");
    return ret;
}

void *detect_thread(void *arg)
{
    (void)arg;
    int lNotify;
    sleep(5);   /*5秒延迟, 等待VimVencChStart()完成*/

    char path[64] = {0};
    strncpy(path, "/sys/devices/virtual/misc/nvp6158c/ahd_input", sizeof(path));

    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        printf("nvp6158c open failed.\n");
        return NULL;
    }


    /*检测dcvi实时状态*/
    int ret = -1;
    unsigned char value[5] = {0};
    unsigned char pos[4] = {4, 6, 0, 2};  /*0xef, 0xbf, 0xfe, 0xfb*/
    int cStatus;

    while (VIM_FALSE == View_Exit)
    {
    	if(vc_get_AHDDetect_status() == 0)
    	{    		
    		usleep(500*1000);
    		continue;
    	}
		else if(vc_get_AHDDetect_status() == 1)
		{
			logd_lc("video config->dur:%d, bit:%d, aud:%d, pho.(%d %d)", 	recInfo.duration, recInfo.bitRate, recInfo.aFmt, 
																		recInfo.vInfo.width, recInfo.vInfo.height);
			vc_set_AHDDetect_status(2);
			dcviChannelMask = 0;
			lNotify = 0;
		}
		
        lseek(fd, 0, SEEK_SET);
        if (read(fd, value, 5) < 0)
        {
            printf("nvp6158c failed to read value, err = %s.\n", strerror(errno));
            break;
        }

        if (dcviChannelMask != atoi(value))
        {
            printf("record channel state change, dcviChannelMask = %#x, newMask = %#x.\n", dcviChannelMask, atoi(value));
            dcviChannelMask = atoi(value);
            for (int i = 0; i < 4; i++)
            {
            	int preStatus;
#ifdef RECORD_DEBUG
                if (((dcviChannelMask >> pos[i]) | 0xfe) == 0xfe)
                {
					ret = VimRecordChStart(i, i, &recInfo);

                    if(ret != 0)
                    {
                        printf("error:VimRecordChStart ch:%d value:%#x ret:%d\n", i, dcviChannelMask >> pos[i], ret);
                    }
					
					cStatus = vc_get_record_status();	
					preStatus = cStatus;
					cStatus |= 1<<i;	
					
					if((cStatus==0x0f)&&(preStatus!=0X0f))
					{
						logd_lc("insert pre:%x, cur:%x", preStatus, cStatus);
						vc_message_send(ENU_LC_DVR_CAM_DEV_ERROR, cStatus, NULL, NULL);
						mCameraLossDetect = 0; /**似乎检测程序不稳定，插着摄像头状态也会变化，所以做个10秒延时检测**/
					}					
					
					vc_set_record_status(cStatus);
					logd_lc("mRecordStatus:%d", cStatus);

					if((cStatus!=0)&&(lNotify==0))
					{					
						vc_message_send(ENU_LC_DVR_NOTIFY_RECORD_STATUS, 1, NULL, NULL);
						lNotify = 1;
					}
                }
                else
                {
                	cStatus = vc_get_record_status();
					preStatus = cStatus;
					cStatus &= ~(1<<i);
					
					if((cStatus!=0x0f)&&(preStatus==0x0f))
					{	/**摄像头前一状态是正常的，当前检测到丢失状态则报错**/
						logd_lc("miss pre:%x, cur:%x", preStatus, cStatus);
						mCameraLossDetect = 25*10;
					}
					
					if(cStatus==0)
					{
						vc_message_send(ENU_LC_DVR_NOTIFY_RECORD_STATUS, 0, NULL, NULL);
					}
					vc_set_record_status(cStatus);					
                }
#endif
            }

            /*DCVI通道有变化需要通知到GUI*/
        }
#ifdef VIM_RECORD_PATH
        /*判断当前剩余存储空间是否小于指定值(200MB)*/
        int loop = 0;
        int freeSize = comm_getDiskFreeSpace(VIDEO_DIR_PATH);
        if (freeSize >= 2000)
            loop = 0;
        else if ((freeSize >= 1600) && (freeSize < 2000))
            loop = 1;
        else if ((freeSize >= 1200) && (freeSize < 1600))
            loop = 10;
        else if ((freeSize >= 800) && (freeSize < 1200))
            loop = 20;
        else if ((freeSize >= 400) && (freeSize < 800))
            loop = 50;
        else
            loop = 100;

        for (int i = 0; i < loop; i++)
        {
            delOldRecordFile();
        }
#endif
        sleep(5);
    }

    close(fd);
    return NULL;
}



int OnStart(void)
{
    int ret = 0;

#ifndef PACK_H265
    printf("onstart\n");
    pthread_mutex_lock(&g_rtspMutex);
    if(g_rtspStartCount == 0)
    {
        printf("vc_record_stop-----------------IN\n");
        vc_record_stop();
        g_rtspStartCount++;
    }
    else
    {
        ret = 1;    /*拒绝请求，限制同时请求数量，目前同时间只支持一路rtsp请求*/
    }
    pthread_mutex_unlock(&g_rtspMutex);
#endif

    return ret;
}

void OnStop(void)
{
#ifndef PACK_H265
    printf("onstop\n");
    pthread_mutex_lock(&g_rtspMutex);
    if(g_rtspStartCount > 0 && g_rtspStartCount-- == 1)
    {
        printf("vc_record_start-----------------IN\n");
        vc_record_start();
    }
    pthread_mutex_unlock(&g_rtspMutex);
#endif
}

extern void test_video_file(char* fileName);
void mediaPlay_start(char *fileName)
{
	pthread_t fielPlayThread;
	int err;
	
	strcpy(playerFilePath, fileName);
	g_player_flag = VIM_TRUE;
	g_player_show = VIM_TRUE;

    err = pthread_create(&fielPlayThread, NULL, mediaPlay_thread, NULL);
    if (0 != err)
    {
        printf("can't create thread: %s\n", strerror(err));
        return -1;
    }
    pthread_detach(fielPlayThread);
}

void mediaPlay_stop(void)
{
	if(g_player_handle>=0)
		OnPlayerStatus(g_player_handle, PLAY_OVER_EVENT, NULL, NULL);
}

void mediaPlay_seek(double time)
{
	if(g_player_handle>=0)
		VimPlayerCtrl(g_player_handle, PLAY_SEEK_CMD, &time);
}

void mediaPlay_pause(int status)
{	
	if(g_player_handle>=0)
	{
		if(status==0)
			VimPlayerCtrl(g_player_handle, PLAY_PAUSE_CMD, NULL);
		else
			VimPlayerCtrl(g_player_handle, PLAY_START_CMD, NULL);
		g_play_status_current = status;
	}
}

void mediaPlay_speed(int speed)
{
	if(g_player_handle>=0)
	{
		VimPlayerCtrl(g_player_handle, PLAY_SPEED_CMD, &speed);
	}
}

void* mediaPlay_information(int *number)
{
	int len = sizeof(int)*2+sizeof(MediaInfo);
	int* s32Ptr = (int*)sys_msg_get_memcache(len);
	s32Ptr[0] = g_play_status_current;
	s32Ptr[1] = g_play_time_current;
	memcpy(&s32Ptr[2], &g_media_Info, len);
	number[0] = len;
	
	return s32Ptr;
}

void *mediaPlay_thread(void *arg)
{
	int s32Ret;

	while(g_player_flag)
	{
#ifndef PACK_H265
		/*SVAC录像回放时需要停止录像*/
		vc_record_stop();
#endif

		MediaInfo mInfo;
		pthread_t feed_thread = 0;

		strncpy(playerFilePath, "/mnt/usb/usb1/test_video_file.mp4", 256);
		int handle = VimPlayerOpen(playerFilePath, &mInfo);
		if(handle <= 0)
		{
			printf("VimPlayerOpen:%s error:%d\n", playerFilePath, handle);
			g_player_flag = VIM_FALSE;
#ifndef PACK_H265
			vc_record_start();
#endif

			/*发送消息让GUI退出播放界面*/
			continue;
		}

		/*发送录像文件时长到GUI*/
		if (handle > 0)
		{
			g_handle = handle;
			printf("send CMD_PLAYER_FILE_SIZE cmd\n");
		}

		s32Ret = vc_vpss_group1_init();
		if (s32Ret != VIM_SUCCESS)
		{
			printf("vpss group1 init failed, error:%d\n", s32Ret);
			g_player_flag = VIM_FALSE;
#ifndef PACK_H265
			vc_record_start();
#endif
			continue;
		}

		VoModInfo vo;
		vo.type = 0;				//绑定vpss模块
		vo.devId = 1;				//vpss模块的groupId
		vo.chId = 1;				//vpss模块group对应的输入通道chid
		vo.vFmt = PIXEL_FMT_NV12;	//vpss模块group对应的输入格式

#ifdef PACK_H265
		s32Ret = VimPlayerStart(handle, &vo, OnPlayerStatus, NULL, NULL);
#else
		s32Ret = VimPlayerStart(handle, &vo, OnPlayerStatus, pwdMd5Str, NULL);
#endif
		if (s32Ret != VIM_SUCCESS)
		{
			printf("VimPlayerStart(%d) error:%d\n", handle, s32Ret);
			vc_vpss_group1_exit();
			g_player_flag = VIM_FALSE;
#ifndef PACK_H265
			vc_record_start();
#endif
			continue;
		}

		//这里必须等待播放器开始事件后才能启动后面的vpss绑定vo或者vpss_feed_vo的线程
		while (playStartEvent == 0 && play_Exit == VIM_FALSE)
		{
			usleep(2000);
		}
		if (playStartEvent == 0)
		{
			printf("play record interrupted, playStartEvent = 0.\n");
			vc_vpss_group1_exit();
			g_player_flag = VIM_FALSE;
#ifndef PACK_H265
			vc_record_start();
#endif
			continue;
		}

		s32Ret = vc_vpss_start_chn();
		ASSERT_RET_RETURN(s32Ret);

		s32Ret = pthread_create(&feed_thread, NULL, (void*)vc_player_handler, NULL);
		if (0 == s32Ret)
		{
			printf("vc_player_handler() create SUCCESS.\n");
			//pthread_detach(feed_thread);
		}
		else
		{
			printf("vc_player_handler() create failed, ret = %d.\n", s32Ret);
			g_player_flag = VIM_FALSE;
#ifndef PACK_H265
			vc_record_start();
#endif
			continue;
		}
		pthread_join(feed_thread, NULL);

		while (VIM_FALSE == play_Exit)
		{
			usleep(10 * 1000);
		}

		VimPlayerClose(g_handle);

		playStartEvent = 0;
		pthread_rwlock_wrlock(&g_vo_rwlock);
		g_player_flag = VIM_FALSE;
		pthread_rwlock_unlock(&g_vo_rwlock);
		play_Exit = VIM_FALSE;

		printf("wait player vpss frame.....\n");
		int waitTimes = 5;
		while(queryVpssFrameList(1) && waitTimes--)
		{
			usleep(10000);
			if(waitTimes == 0)
			{
				releaseVpssFrameList(1, 4);
				break;
			}
		}
		
		vc_message_send(ENU_LC_DRV_MEDIA_PLAY_REPORT, 0, NULL, NULL);
		printf("VimPlayerClose[%d].\n", __LINE__);
		vc_vpss_group1_exit();
		printf("playback end[%d].\n", __LINE__);
#ifndef PACK_H265
		vc_record_start();
#endif
	}
}


void *mdvr_thread_handle(void *arg)
{
	VIM_S8 *filename = NULL;
	sample_config config;
	char* tempArg;
	int err;
	VIM_S32 s32Ret = 0, i = 0;
	SAMPLE_ENTER();
	signal(SIGINT, sample_HandleSig);
    signal(SIGTERM, sample_HandleSig);
    signal(SIGQUIT, sample_HandleSig);

    pthread_rwlock_init(&g_vo_rwlock, NULL);

	memset(&config,0x0,sizeof(sample_config));
	{
		strcpy(config.dcvi_config.name[i],"/system/app/cfg/bt1120_dcvi.cfg");	// -d
		config.dcvi_config.num++;
		strcpy(config.vo_config.name[i],"/system/app/cfg/vo_mptx_1080p_2X2.cfg");		// -o
		config.vo_config.num++;
		strcpy(config.vpss_config.name[i],"/system/app/cfg/vpss_dvr.cfg");	// -v
		config.vpss_config.num++;
		strcpy(config.venc_config.name[i],"/system/app/cfg/venc_h265_1080P_4chn.cfg");	// -f
		config.venc_config.num++;		
	}
#ifdef LC_HAVE_ALGORITHEM

	
	gDmsAlgorithemImgList[0] = (char*)malloc(LC_ALGORITEM_SIZE*LC_ALGORITEM_SIZE*3*8);
	gAdasAlgorithemImgList[0] = gDmsAlgorithemImgList[0]+LC_ALGORITEM_SIZE*LC_ALGORITEM_SIZE*3*4;
	for(int i=1; i<4; i++)
	{
		gDmsAlgorithemImgList[i] = gDmsAlgorithemImgList[i-1]+LC_ALGORITEM_SIZE*LC_ALGORITEM_SIZE*3;
		gAdasAlgorithemImgList[i] = gAdasAlgorithemImgList[i-1]+LC_ALGORITEM_SIZE*LC_ALGORITEM_SIZE*3;	
	}
	
	
	/*
	gDmsAlgorithemImgList[0] = (char*)malloc(LC_ALGORITEM_SIZE_ADAS_width*LC_ALGORITEM_SIZE_ADAS_height*3*8);
	gAdasAlgorithemImgList[0] = gDmsAlgorithemImgList[0]+LC_ALGORITEM_SIZE_ADAS_width*LC_ALGORITEM_SIZE_ADAS_height*3*4;
	for(int i=1; i<4; i++)
	{
		gDmsAlgorithemImgList[i] = gDmsAlgorithemImgList[i-1]+LC_ALGORITEM_SIZE_ADAS_width*LC_ALGORITEM_SIZE_ADAS_height*3;
		gAdasAlgorithemImgList[i] = gAdasAlgorithemImgList[i-1]+LC_ALGORITEM_SIZE_ADAS_width*LC_ALGORITEM_SIZE_ADAS_height*3;	
	}
	*/

	gDmsImgIndexHead = 0;
	gDmsImgIndexTail = 0;
	gAdasImgIndexHead = 0;
	gAdasImgIndexTail = 0;
	sem_init(&semAlgorithem, 0, 0);
	mDmsFrameRate = 0;
	mAdasFrameRate = 0;

	//鍒濆鍖栨ā鍨?	if(Dms_Switch)
		{
			lc_dvr_bridge_dms_initial();
		}
	else if(Adas_Switch)
		{
			lc_dvr_bridge_adas_initial();
		}

#endif

#ifdef VIM_RECORD_PATH
    if (0 != access(RECORD_DIR_PATH, F_OK))
        mkdir(RECORD_DIR_PATH, 0777);

    if (0 != access(VIDEO_DIR_PATH, F_OK))
        mkdir(VIDEO_DIR_PATH, 0777);

    if (0 != access(ALARM_DIR_PATH, F_OK))
        mkdir(ALARM_DIR_PATH, 0644);
#endif
    /*读取config.ini并得到录像加密密码*/
    char *pwd = NULL;
    dictionary *ini = NULL;
    ini = iniparser_load("/system/config.ini");
    if (ini == NULL)
    {
        printf("cannot found config.ini.\n");
        return -1;
    }
    pwd = iniparser_getstring(ini, "GBPLAT_SETTING:VIDEO_ENCRYPT_PWD", NULL);
    StringMd5Sum(pwd, pwdMd5Str);
    printf("get video password sucessful, pwd = %.32s, pwdMd5Str = %.34s.\n", pwd, pwdMd5Str);

	gVbConfig.u32MaxPoolCnt = 1;
	gVbConfig.astCommPool[0].u32BlkSize = 0;
	gVbConfig.astCommPool[0].u32BlkCnt = 0;

	/*----------Start of sys init----------*/
	s32Ret = SAMPLE_COMM_SYS_Init(&gVbConfig);
	if (s32Ret != VIM_SUCCESS ) {
		SAMPLE_PRT( "failed, s32Ret = %d\n", s32Ret );
	}

	if(config.dcvi_config.num == 0 //|| config.vo_config.num == 0
	 || config.vpss_config.num == 0){
		goto sys_deinit;
	}

	printf("dcvi_config_file = %s \r\n",config.dcvi_config.name[0]);
	printf("vo_config_file = %s \r\n",config.vo_config.name[0]);
	printf("vpss_config_file = %s \r\n",config.vpss_config.name[0]);

    /*录像SDK初始化*/
#ifdef VIM_RECORD_PATH	
#ifdef PACK_H265
    s32Ret = VimRecordInit(".", GetRecFullPath, NULL, 0, NULL, NULL);
#else
    s32Ret = VimRecordInit(".", GetRecFullPath, NULL, 1, pwdMd5Str, NULL);
#endif
#else
#ifdef PACK_H265
	s32Ret = VimRecordInit(".", vim_record_filePath, (ExtRwHanlder*)(vc_vim_file_handle()), 0, NULL, NULL); 
#else
    s32Ret = VimRecordInit(".", vim_record_filePath, (ExtRwHanlder*)(vc_vim_file_handle()), 1, pwdMd5Str, NULL);
#endif
#endif
	VimSetDeviceAudioCb(lc_AudioFrameCb, NULL);
    if (VIM_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("VimRecordInit() failed. (%d)\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return VIM_FAILURE;
    }

    pthread_mutex_init(&g_rtspMutex, NULL);

//    RtspEventHandler handle = {0, 0};
//    handle.OnStart = OnStart;
//    handle.OnStop = OnStop;
//    s32Ret = VimRtspStart(8554, handle);
//    if (0 != s32Ret)
//    {
//        printf("rtsp start fail, ret = %d\n", s32Ret);
//        return 0;
//    }
//    printf("rtsp start success\n");

    memset(&recInfo, 0, sizeof(recInfo));
//=================================
	recInfo.duration = LC_DVR_DURATION;
	recInfo.bitRate = LC_DVR_BITRATE;
	recInfo.vInfo.pixelFmt = PIXEL_FMT_NV12;
	if(vc_record_video_para_get(VC_IDX_PHOTOSZ)==1)
	{
		recInfo.vInfo.width = LC_RECORD_VIDEO_NSIZE_W; //1920;//3840;
		recInfo.vInfo.height = LC_RECORD_VIDEO_NSIZE_H; //1080;//2160;	
	}
	else
	{
		recInfo.vInfo.width = LC_RECORD_VIDEO_HSIZE_W; //1920;//3840;
		recInfo.vInfo.height = LC_RECORD_VIDEO_HSIZE_H; //1080;//2160;
	}
    recInfo.vInfo.frameRate = 25;
	recInfo.aFmt = AV_FMT_AAC;
	recInfo.aInfo.sampleRate = 16000;//16000; /**48000 播放不正，速度过快**/
	recInfo.aInfo.channel = 1;
	recInfo.aInfo.sampleFmt = SAMPLE_BIT_S16_LE;	
#ifdef PACK_H265
    recInfo.vFmt = AV_FMT_H265;
    recInfo.vEncConfig = 0;
#else
    recInfo.vFmt = AV_FMT_SVAC2;
    recInfo.vEncConfig = 1;
#endif
//=================================
	s32Ret = TestView(&config);
	if (VIM_SUCCESS != s32Ret){
		SAMPLE_PRT(" DO_UBINDER_Exit failed 0x%x\n", s32Ret);
	}
	
    /*AHD摄像头侦测线程*/
    pthread_t ntid;
    err = pthread_create(&ntid, NULL, detect_thread, NULL);
    if (0 != err)
    {
        printf("can't create thread: %s\n", strerror(err));
        return -1;
    }
    pthread_detach(ntid);

    /*OSD线程*/
#ifdef OSD_ENABLE
    pthread_t pid;
	sleep(1);
    err = pthread_create(&pid, NULL, osd_thread, NULL);
    if (0 != err)
    {
        printf("can't create thread: %s\n", strerror(err));
        return -1;
    }
    pthread_detach(pid);
#endif

    while(1)
    {
        sleep(1);
    }

	VimRecordUnInit();
	sem_destroy(&semAlgorithem);

sys_deinit:
	SAMPLE_COMM_SYS_Exit();
	SAMPLE_LEAVE();
	
#ifdef LC_HAVE_ALGORITHEM
	// 鍙嶅垵濮嬪寲妯″瀷
	if(Dms_Switch)
		{
			lc_dvr_bridge_dms_unInitial();
			free(gDmsAlgorithemImgList[0]);
		}
	if(Adas_Switch)
		{
			lc_dvr_bridge_adas_unInitial();
			free(gAdasAlgorithemImgList[0]);
		}
	
#endif	
	
	return NULL;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
