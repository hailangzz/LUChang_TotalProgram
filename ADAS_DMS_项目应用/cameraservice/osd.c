/*****************************************************************
* Copyright (C), 2001-2020 Beijing Vimicro Technology Co., Ltd.
* ALL Rights Reserved
*
* Name      : socket_comm.c
* Summary   :
* Date      : 2022.8.27
* Author    :
*
*****************************************************************/

#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include "osd.h"
#include "sample_common_vpss.h"


#ifdef __cplusplus
    extern "C" {
#endif

#define RGN_RGB888_RED          0x00FF0000
#define RGN_RGB888_BLUE         0x000000FF
#define RGN_RGB888_GREEN        0x0000FF00
#define RGN_RGB888_YELLOW       0x00FFFF00
#define RGN_RGB888_BLACK        0x00000000
#define RGN_RGB888_WRITE        0x00FFFFFF
#define RGN_RGB888_PURPLE       0x009900FF
#define RGN_RGB888_PINK         0x00FFCCFF
#define RGN_RGB888_SKYBLUE      0x0033FFFF

#define RGN_YUV10BIT(y, u, v)       ((v << 22) | (u << 12) | (y << 2))
#define RGN_CHN_MAX_NUM             4   // ��ͨ������region����
#define RGN_FRAMETIME_NUM           5   // ��ͨ������region����
#define RGN_COVER_HANDLE_START      0   // COVER��ʼ�����ÿ��ͨ������4����16��ͨ�����ܹ�64
#define RGN_OSD_HANDLE_START        70  // OSD��ʼ�����ÿ��ͨ������4����6��ͨ����16��grp, �ܹ�384
#define RGN_FRAMETIME_HANDLE_START  460 // OSD��ʼ�����ÿ��ͨ������2����6��ͨ����16��grp, �ܹ�192

#define OSD_LICENSE_WIDTH  160
#define OSD_TIME_WIDTH  352
#define OSD_SPEED_WIDTH  160
#define OSD_FONT_HEIGHT  32

#define MAX_VPSS_CHN  6
extern SAMPLE_MAN_INFO_S mstManInfo;
extern  VIM_VPSS_GROUP_ATTR_S  grpAttr[16];
extern VIM_VPSS_INPUT_ATTR_S  chnInAttr[16];
extern VIM_CHN_CFG_ATTR_S     chnOutAttr[16][MAX_VPSS_CHN];
int mOsdFontLoad = -1;

extern int lc_dvr_mcu_get_speed(void);
/******************************************************************************
 * function : Region & OSD
 ******************************************************************************/
VIM_S32 dvr_DO_REGION_OSD_Init(VIM_S32 s32GrpId, VIM_S32 s32ChnId,
        VIM_U32 u32Width, VIM_U32 u32Height, VIM_CHAR *sTestString, VIM_BOOL bInvert)
{
    int num = 0;
    VIM_S32 s32Ret;

    /*1. ����ͨ������*/
    RGN_CHN_CONFIG_S stRgnChnConf;
    memset(&stRgnChnConf, 0x00, sizeof(RGN_CHN_CONFIG_S));
    stRgnChnConf.enType = OSD_RGN;
	stRgnChnConf.rgn_mode = RGN_MODE_STATIC;
    stRgnChnConf.ImgSize.u32Width = u32Width;       /*VPSS ʵ�����ͼ���С*/
    stRgnChnConf.ImgSize.u32Height = u32Height;     /*VPSS ʵ�����ͼ���С*/
    for (num = 0; num < 4; num++)
    {
        stRgnChnConf.Vid = num;                     /*Vid��ӦDCVIͨ��0, 1, 2, 3*/
        s32Ret = VIM_MPI_RGN_SetChannelConfig(s32GrpId, s32ChnId, &stRgnChnConf);
        ASSERT_RET_RETURN(s32Ret);
    }

    /*2. ����ͨ������ʾ���Բ���*/
    RGN_CHN_ATTR_S stRgnChnAttr;
    memset(&stRgnChnAttr, 0x00, sizeof(RGN_CHN_ATTR_S));

    stRgnChnAttr.enType = OSD_RGN;
    stRgnChnAttr.rgn_mode = RGN_MODE_STATIC;//��Ӧ�����ṹ���޸�,�˴�Ϊģʽ˵����ԭ�ӿڵ��ò���Ҳ���Ե��ã���generalģʽ��Ĭ��Ϊ0
    stRgnChnAttr.uChnAttr.stOsdVi.u32Color0 = DO_VI_GetRGB_YUV_10Bit(RGN_RGB888_WRITE);
    stRgnChnAttr.uChnAttr.stOsdVi.u32Color1 = DO_VI_GetRGB_YUV_10Bit(RGN_RGB888_BLACK);
//    stRgnChnAttr.uChnAttr.stOsdVi.u32Color2 = DO_VI_GetRGB_YUV_10Bit(RGN_RGB888_BLUE);
    stRgnChnAttr.uChnAttr.stOsdVi.u32Color2 = DO_VI_GetRGB_YUV_10Bit(mstManInfo.u32OsdColor);   /*cfg�ļ���ָ��*/
    SAMPLE_PRT("config color u32OsdColor:0x%x,config framerate u32OsdOpen:0x%x\n", mstManInfo.u32OsdColor, mstManInfo.u32OsdOpen);

    stRgnChnAttr.uChnAttr.stOsdVi.u32Alpha = 0x40;
    stRgnChnAttr.uChnAttr.stOsdVi.enPixelFmt = PIXEL_FMT_RAW_2BPP;
    stRgnChnAttr.uChnAttr.stOsdVi.stRgnOps.bEnable = VIM_TRUE;
    stRgnChnAttr.uChnAttr.stOsdVi.stInvertColor.u32InvClrControlVal = 0x00;

    s32Ret = VIM_MPI_RGN_SetChannelDisplayAttr(s32GrpId, s32ChnId, &stRgnChnAttr);
    ASSERT_RET_RETURN(s32Ret);

    /*3. ��������*/
    VIM_U32 i, u32Start;
    VIM_U32 u32w, u32h;
    RGN_ATTR_S stRgnAttr[12];    /*vpss��dcvi��4��ͨ��, ���Դ���8������, ǰ4����4��ͨ�������Ͻ���ʾͨ����, ��4����4��ͨ�������Ͻ���ʾLOGO*/
    memset(stRgnAttr, 0x00, sizeof(stRgnAttr));

    /*������������δʹ�õ� Handle ��*/
    u32Start = RGN_OSD_HANDLE_START + (s32GrpId * RGN_CHN_MAX_NUM * MAX_VPSS_CHN) + (s32ChnId * RGN_CHN_MAX_NUM * 4);
    i = u32Start;

    for (num = 0; num < 12; num++, i++)
    {
    	int px, py, num_off;
		
        if (num < 4)
        {
            u32w = OSD_LICENSE_WIDTH; /**���Ƶ�λ��**/
            u32h = OSD_FONT_HEIGHT;
			px = u32Width-u32w;
			py = 8;
			num_off = 0;
        }
        else if(num < 8)
        {
            u32w = OSD_TIME_WIDTH;	/**ʱ�估ͨ��λ��**/
            u32h = OSD_FONT_HEIGHT;
			px = 8;
			py = 8;	
			num_off = 4;
        }
		else
		{
            u32w = OSD_SPEED_WIDTH; /**�����ٶ���ʾ**/
            u32h = OSD_FONT_HEIGHT;	
			px = u32Width-u32w;
			py = u32Height-u32h;	
			num_off = 8;
		}

        stRgnAttr[num].enType = OSD_RGN;
        stRgnAttr[num].enPixelFmt = PIXEL_FMT_RAW_2BPP;
        stRgnAttr[num].stSize.u32Width = u32w;       /*OSD��ʾ���*/
        stRgnAttr[num].stSize.u32Height = u32h;      /*OSD��ʾ�߶�*/
        stRgnAttr[num].bindSate = 0;

        //Allow to specify the location when creating a region
        s32Ret = VIM_MPI_RGN_CreateRegion(s32GrpId, i, &stRgnAttr[num]);
        ASSERT_RET_RETURN(s32Ret);


        /*4. ��������̬����*/
		stRgnAttr[num].stPoint.s32X = px;	 /*OSD��ʾ��ʼX����*/
		stRgnAttr[num].stPoint.s32Y = py;	 /*OSD��ʾ��ʼY����*/

        s32Ret = VIM_MPI_RGN_SetRegionAttr(s32GrpId, i, &stRgnAttr[num]);
        ASSERT_RET_RETURN(s32Ret);

        /*5. ������ӵ�ͨ��*/
        s32Ret = VIM_MPI_RGN_AttachToChnTsm(s32GrpId, s32ChnId, num - num_off, i);
            
        if (s32Ret != VIM_SUCCESS)
        {
            VIM_MPI_RGN_DestroyRegion(s32GrpId, i);
            ASSERT_RET_RETURN(s32Ret);
        }
    }


    /*6. ��������λͼ����*/
    /*6.1 ���������ļ�*/
	if(mOsdFontLoad<0)
	{
		mOsdFontLoad = 2;
	    s32Ret = SAMPLE_RGN_LoadFont(mOsdFontLoad);
	    ASSERT_RET_RETURN(s32Ret);
	}

    /*6.2 ����λͼ*/
    VIM_U32 u32RgnBitLen = 0;
    RGN_BITMAP_S stBitmap;
    memset(&stBitmap, 0x00, sizeof(RGN_BITMAP_S));
	u32RgnBitLen = OSD_TIME_WIDTH * OSD_FONT_HEIGHT * 2 / 8;
	
    stBitmap.enPixelFormat = stRgnAttr[0].enPixelFmt;
	stBitmap.u32Width = OSD_LICENSE_WIDTH;  /*bitmap��osd�����ͬ*/
    stBitmap.u32Height = OSD_FONT_HEIGHT;   /*bitmap��osd�����ͬ*/
    
    
    stBitmap.pData = (VIM_VOID *)malloc(u32RgnBitLen);

    for (num = 0, i = u32Start; num < 4; num++, i++)
    {
        memset(stBitmap.pData, 0x00, u32RgnBitLen);
        SAMPLE_RGN_FillStringToBitmapBuffer(&stBitmap, &sTestString[0], 1);     /*���ͨ����ɫ1*/

        /*6.3 ��λͼ������䵽OSD*/
        s32Ret = VIM_MPI_RGN_SetRegionBitMap(s32GrpId, i, &stBitmap);
        ASSERT_RET_RETURN(s32Ret);

        /*7. ��ʾOSD*/
        s32Ret = VIM_MPI_RGN_ShowRegion(s32GrpId, i);
        ASSERT_RET_RETURN(s32Ret);
    }

    struct tm *lt;
    struct timeval tv;
    struct timezone tz;
	char osdShow[64];
	
	gettimeofday(&tv, &tz);
	lt = localtime(&tv.tv_sec); 
	stBitmap.u32Width = OSD_TIME_WIDTH;  /*bitmap��osd�����ͬ*/
    stBitmap.u32Height = OSD_FONT_HEIGHT; /*bitmap��osd�����ͬ*/	

    for (num = 0; num < 4; num++, i++)
    {
        memset(stBitmap.pData, 0x00, u32RgnBitLen);
		sprintf(osdShow, "%d %02d-%02d-%02d %02d:%02d:%02d\n", num, lt->tm_year + 1900,
				lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);

        SAMPLE_RGN_FillStringToBitmapBuffer(&stBitmap, &osdShow[0], 1);     /*���ͨ����ɫ1*/

        /*6.3 ��λͼ������䵽OSD*/
        s32Ret = VIM_MPI_RGN_SetRegionBitMap(s32GrpId, i, &stBitmap);
        ASSERT_RET_RETURN(s32Ret);

        /*7. ��ʾOSD*/
        s32Ret = VIM_MPI_RGN_ShowRegion(s32GrpId, i);
        ASSERT_RET_RETURN(s32Ret);
    }

	stBitmap.u32Width = OSD_SPEED_WIDTH;  /*bitmap��osd�����ͬ*/
    stBitmap.u32Height = OSD_FONT_HEIGHT;  /*bitmap��osd�����ͬ*/	
    for (num = 0; num < 4; num++, i++)
    {
        memset(stBitmap.pData, 0x00, u32RgnBitLen);
		sprintf(osdShow, "46 km/h");
        SAMPLE_RGN_FillStringToBitmapBuffer(&stBitmap, &osdShow[0], 1);     /*���ͨ����ɫ1*/

        /*6.3 ��λͼ������䵽OSD*/
        s32Ret = VIM_MPI_RGN_SetRegionBitMap(s32GrpId, i, &stBitmap);
        ASSERT_RET_RETURN(s32Ret);

        /*7. ��ʾOSD*/
        s32Ret = VIM_MPI_RGN_ShowRegion(s32GrpId, i);
        ASSERT_RET_RETURN(s32Ret);
    }

    /*������Ҫ�ͷ������ļ�*/
    if (stBitmap.pData != NULL)
    {
        free(stBitmap.pData);
        stBitmap.pData = NULL;
    }

    return s32Ret;
}		

void osd_reset_license(char *license)
{
	VIM_S32 s32GrpId = 0;
	VIM_S32 s32ChnId = 0;
	VIM_U32 u32Start, i;
	VIM_U32 u32w = 160, u32h = 32;
	VIM_S32 s32Ret;
	char sTestString[64];
    VIM_U32 u32FontSize = 2;

	printf("osd reset license: %s\n", license);
	for(s32ChnId = 0; s32ChnId<MAX_VPSS_CHN; s32ChnId++)
	{
		if(! ((mstManInfo.u32ChnOutMask[s32GrpId]>>s32ChnId) & 0x1))
			continue;

		/*������������δʹ�õ� Handle ��*/
		u32Start = RGN_OSD_HANDLE_START + (s32GrpId * RGN_CHN_MAX_NUM * MAX_VPSS_CHN) + (s32ChnId * RGN_CHN_MAX_NUM * 4);
		i = u32Start;

		/*6.2 ����λͼ*/
		VIM_U32 u32RgnBitLen = 0;
		RGN_BITMAP_S stBitmap;
		memset(&stBitmap, 0x00, sizeof(RGN_BITMAP_S));

		stBitmap.enPixelFormat = PIXEL_FMT_RAW_2BPP;
		stBitmap.u32Height = OSD_FONT_HEIGHT;	  /*bitmap��osd�����ͬ*/
		stBitmap.u32Width = OSD_LICENSE_WIDTH;	  /*bitmap��osd�����ͬ*/
		u32RgnBitLen = OSD_TIME_WIDTH * OSD_FONT_HEIGHT * 2 / 8;
		stBitmap.pData = (VIM_VOID *)malloc(u32RgnBitLen);
		if (stBitmap.pData == NULL)
		{
			s32Ret = VIM_ERR_RGN_NOMEM;
		}

		memset(stBitmap.pData, 0x00, u32RgnBitLen);
		sprintf(sTestString, "%s", license);		
		SAMPLE_RGN_FillStringToBitmapBuffer(&stBitmap, &sTestString[0], 1); 	//���ͨ����ɫ1
		
		/*6.3 ��λͼ������䵽OSD*/
		for(int num=0; num<4; num++, i++)
		{
			s32Ret = VIM_MPI_RGN_SetRegionBitMap(s32GrpId, i, &stBitmap);
			ASSERT_RET_RETURN(s32Ret);
			
			s32Ret = VIM_MPI_RGN_ShowRegion(s32GrpId, i);
			ASSERT_RET_RETURN(s32Ret);
		}
		
		if (stBitmap.pData != NULL)
		{
			free(stBitmap.pData);
		}		
	}	
}

void *osd_thread(void *arg)
{
    (void)arg;
    VIM_S32 s32Ret = -1;
    VIM_S32 s32GrpId = 0;
    VIM_S32 s32ChnId = 0;
    struct tm *lt;
    struct timeval tv;
    struct timezone tz;
    VIM_CHAR sTestString[64] = {0};
    VIM_U32 u32Start, u32RgnBitLen = 0;
	RGN_BITMAP_S stBitmap;
	int i, num;    
	printf("osd_thread start!\n");
	memset(&stBitmap, 0x00, sizeof(RGN_BITMAP_S));	
	stBitmap.enPixelFormat = PIXEL_FMT_RAW_2BPP;			
	u32RgnBitLen = OSD_TIME_WIDTH * OSD_FONT_HEIGHT * 2 / 8;
	stBitmap.pData = (VIM_VOID *)malloc(u32RgnBitLen);

    while(1)
    {
		for(s32ChnId = 0; s32ChnId<MAX_VPSS_CHN; s32ChnId++)
		{
			if(!((mstManInfo.u32ChnOutMask[s32GrpId]>>s32ChnId) & 0x1))
				continue;

			/*������������δʹ�õ� Handle ��*/
			u32Start = RGN_OSD_HANDLE_START + (s32GrpId * RGN_CHN_MAX_NUM * MAX_VPSS_CHN) + (s32ChnId * RGN_CHN_MAX_NUM * 4);
			i = u32Start+4;

			/*6.2 ����λͼ*/
			stBitmap.u32Width = OSD_TIME_WIDTH;   /*bitmap��osd�����ͬ*/
			stBitmap.u32Height = OSD_FONT_HEIGHT;	  /*bitmap��osd�����ͬ*/

			gettimeofday(&tv, &tz);
			lt = localtime(&tv.tv_sec); 						
			
			/*6.3 ��λͼ������䵽OSD*/
			for(int num=0; num<4; num++, i++)
			{
		        memset(stBitmap.pData, 0x00, u32RgnBitLen);
				sprintf(sTestString, "%d %02d-%02d-%02d %02d:%02d:%02d\n", num, lt->tm_year + 1900,
						lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
				SAMPLE_RGN_FillStringToBitmapBuffer(&stBitmap, &sTestString[0], 1); 	//���ͨ����ɫ1
		
				s32Ret = VIM_MPI_RGN_SetRegionBitMap(s32GrpId, i, &stBitmap);
				ASSERT_RET_RETURN(s32Ret);
				
				s32Ret = VIM_MPI_RGN_ShowRegion(s32GrpId, i);
				ASSERT_RET_RETURN(s32Ret);
			}

			stBitmap.u32Width = OSD_SPEED_WIDTH;  /*bitmap��osd�����ͬ*/
		    stBitmap.u32Height = OSD_FONT_HEIGHT;  /*bitmap��osd�����ͬ*/	
	        memset(stBitmap.pData, 0x00, u32RgnBitLen);
			sprintf(sTestString,"%03d km/h\n", lc_dvr_mcu_get_speed());

	        SAMPLE_RGN_FillStringToBitmapBuffer(&stBitmap, &sTestString[0], 1);     /*���ͨ����ɫ1*/
				
		    for (num = 0; num < 4; num++, i++)
		    {
		        /*6.3 ��λͼ������䵽OSD*/
		        s32Ret = VIM_MPI_RGN_SetRegionBitMap(s32GrpId, i, &stBitmap);
		        ASSERT_RET_RETURN(s32Ret);
		        /*7. ��ʾOSD*/
		        s32Ret = VIM_MPI_RGN_ShowRegion(s32GrpId, i);
		        ASSERT_RET_RETURN(s32Ret);
		    }
		}        
        usleep(500 * 1000);
    }
	
	if (stBitmap.pData != NULL)
	{
		free(stBitmap.pData);
	}	
	/*������Ҫ�ͷ������ļ�*/
	if(mOsdFontLoad>0)
	{
		SAMPLE_RGN_ReleaseFont(mOsdFontLoad);	
		mOsdFontLoad = -1;
	}
}
#ifdef __cplusplus
    }
#endif


