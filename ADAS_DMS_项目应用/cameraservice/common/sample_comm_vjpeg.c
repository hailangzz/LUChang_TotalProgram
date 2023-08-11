#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif

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
#include <unistd.h>

#include "sample_comm_vjpeg.h"
#include "sample_venc_cfgParser.h"

#define SAMPLE_SYS_ALIGN_WIDTH 32

#ifndef CEILING_2_POWER
#define CEILING_2_POWER(x, a) (((x) + ((a)-1)) & (~((a)-1)))
#endif

typedef struct jpeg_venc_getstream_s
{
    VIM_S32 s32Cnt;
    VIM_BOOL bThreadStart;
} JPEG_VENC_GETSTREAM_PARA_S;

static JPEG_VENC_GETSTREAM_PARA_S gs_stPara = {0};
static pthread_t gs_VencPid;

/******************************************************************************
 * session : cfg file parse
 ******************************************************************************/

static VIM_S32 ParsePixFmt(char *format, PIXEL_FORMAT_E *penPixFmt)
{
    if (!strcasecmp(format, "yuv400"))
    {
        *penPixFmt = PIXEL_FORMAT_YUV_400;
    }
    else if (!strcasecmp(format, "yuv420p"))
    {
        *penPixFmt = PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcasecmp(format, "yuv422p"))
    {
        *penPixFmt = PIXEL_FORMAT_YUV_PLANAR_422;
    }
    else if (!strcasecmp(format, "yuv444p"))
    {
        *penPixFmt = PIXEL_FORMAT_YUV_PLANAR_444;
    }
    else if (!strcasecmp(format, "yuv420sp"))
    {
        *penPixFmt = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcasecmp(format, "yuv422sp"))
    {
        *penPixFmt = PIXEL_FORMAT_YUV_SEMIPLANAR_422;
    }
    else if (!strcasecmp(format, "yuv444sp"))
    {
        *penPixFmt = PIXEL_FORMAT_YUV_SEMIPLANAR_444;
    }
    else
    {
        SAMPLE_PRT("unsupported encode format: %s!\n", format);
        return -1;
    }
    return 0;
}

static VIM_S32 parseJpegCfg(JPEG_CHN_CFG_S *pstChnCfg,
								 char *FileName,
								 unsigned int offset,
								 unsigned int offset_end)
{
    FILE *fp = NULL;
    char sValue[256] = {0};
    int iValue = 0;
    int ret = 0;

    fp = fopen(FileName, "r");
    if (fp == NULL)
    {
        SAMPLE_PRT("file open err : %s, errno(%d)\n", FileName, errno);
        return ret;
    }

    if (WAVE_GetValue_New(fp, "chnIdx", &iValue, offset, offset_end))
    {
        pstChnCfg->chn = iValue;
    }
    if (WAVE_GetValue_New(fp, "gap", &iValue, offset, offset_end))
    {
        pstChnCfg->gap = iValue;
    }
    if (WAVE_GetValue_New(fp, "repeats", &iValue, offset, offset_end))
    {
        pstChnCfg->repeats = iValue;
    }
    if (WAVE_GetValue_New(fp, "inputMode", &iValue, offset, offset_end))
    {
        pstChnCfg->input_mode = iValue;
    }
    if (WAVE_GetValue_New(fp, "save", &iValue, offset, offset_end))
    {
        pstChnCfg->save = iValue;
    }
    if (WAVE_GetValue_New(fp, "chn_vpss", &iValue, offset, offset_end))
    {
        pstChnCfg->chn_vpss = iValue;
    }
    memset(sValue, 0x0, sizeof(sValue));
    if (WAVE_GetStringValue(fp, "inputFile", (char *)&sValue, offset))
    {
        strcpy(pstChnCfg->input_file, sValue);
    }

    JPEG_CHN_ATTR_S * pstChnAttr = &pstChnCfg->stChnAttr;

    if (WAVE_GetValue_New(fp, "u32FrameGap", &iValue, offset, offset_end))
    {
        pstChnAttr->u32FrameGap = iValue;
    }

    if (WAVE_GetValue_New(fp, "decode", &iValue, offset, offset_end))
    {
        pstChnAttr->decode = iValue;
    }

    if (WAVE_GetValue_New(fp, "QLevel", &iValue, offset, offset_end))
    {
        pstChnAttr->u32QLevel = iValue;
    }

    if (WAVE_GetValue_New(fp, "u32HorScale", &iValue, offset, offset_end))
    {
        pstChnAttr->u32HorScale = iValue;
    }
    if (WAVE_GetValue_New(fp, "u32VerScale", &iValue, offset, offset_end))
    {
        pstChnAttr->u32VerScale = iValue;
    }

    memset(sValue, 0x0, sizeof(sValue));
    if (WAVE_GetStringValue(fp, "format", (char *)&sValue, offset))
    {
        ParsePixFmt(sValue, &pstChnAttr->enPixFmt);
    }
    if (WAVE_GetValue_New(fp, "picWidth", &iValue, offset, offset_end))
    {
        pstChnAttr->u32PicWidth = iValue;
    }
    if (WAVE_GetValue_New(fp, "picHeight", &iValue, offset, offset_end))
    {
        pstChnAttr->u32PicHeight = iValue;
    }
    if (WAVE_GetValue_New(fp, "rotation", &iValue, offset, offset_end))
    {
        switch (iValue)
        {
        case 90:
            pstChnAttr->enRotate = ROTATE_90;
            break;
        case 180:
            pstChnAttr->enRotate = ROTATE_180;
            break;
        case 270:
            pstChnAttr->enRotate = ROTATE_270;
            break;
        }
    }
    if (WAVE_GetValue_New(fp, "mirror", &iValue, offset, offset_end))
    {
        switch (iValue)
        {
        case 1:
            pstChnAttr->enMirror = VENC_MIRDIR_VER;
            break;
        case 2:
            pstChnAttr->enMirror = VENC_MIRDIR_HOR;
            break;
        case 3:
            pstChnAttr->enMirror = VENC_MIRDIR_HOR_VER;
            break;
        }
    }
    if (WAVE_GetValue_New(fp, "crop", &iValue, offset, offset_end))
    {
        if (iValue > 0)
        {
            if (WAVE_GetValue_New(fp, "cropX", &iValue, offset, offset_end))
            {
                pstChnAttr->stCropRect.s32X = iValue;
            }
            if (WAVE_GetValue_New(fp, "cropY", &iValue, offset, offset_end))
            {
                pstChnAttr->stCropRect.s32Y = iValue;
            }
            if (WAVE_GetValue_New(fp, "cropW", &iValue, offset, offset_end))
            {
                pstChnAttr->stCropRect.u32Width = iValue;
            }
            if (WAVE_GetValue_New(fp, "cropH", &iValue, offset, offset_end))
            {
                pstChnAttr->stCropRect.u32Height = iValue;
            }
        }
    }

    fclose(fp);
    return ret;
}

static VIM_S32 printJpegCfg(JPEG_CHN_CFG_S *pstChnCfg)
{
    SAMPLE_PRT("======== chn%d dump ========\n", pstChnCfg->chn);
    SAMPLE_PRT("save        : %d\n", pstChnCfg->save);
    SAMPLE_PRT("gap         : %d\n", pstChnCfg->gap);
    SAMPLE_PRT("vpssChn     : %d\n", pstChnCfg->chn_vpss);
    SAMPLE_PRT("repeats     : %d\n", pstChnCfg->repeats);
    SAMPLE_PRT("inputMode   : %d\n", pstChnCfg->input_mode);
    SAMPLE_PRT("inputFile   : %s\n", pstChnCfg->input_file);
    SAMPLE_PRT("frameGap    : %d\n", pstChnCfg->stChnAttr.u32FrameGap);
    SAMPLE_PRT("format      : %d\n", pstChnCfg->stChnAttr.enPixFmt);
    SAMPLE_PRT("picWidth    : %d\n", pstChnCfg->stChnAttr.u32PicWidth);
    SAMPLE_PRT("picHeight   : %d\n", pstChnCfg->stChnAttr.u32PicHeight);
    SAMPLE_PRT("mirror      : %d\n", pstChnCfg->stChnAttr.enMirror);
    SAMPLE_PRT("decode      : %d\n", pstChnCfg->stChnAttr.decode);
    SAMPLE_PRT("QLevel      : %d\n", pstChnCfg->stChnAttr.u32QLevel);
    SAMPLE_PRT("rotation    : %d\n", pstChnCfg->stChnAttr.enRotate);
    SAMPLE_PRT("cropX       : %d\n", pstChnCfg->stChnAttr.stCropRect.s32X);
    SAMPLE_PRT("cropY       : %d\n", pstChnCfg->stChnAttr.stCropRect.s32Y);
    SAMPLE_PRT("cropW       : %d\n", pstChnCfg->stChnAttr.stCropRect.u32Width);
    SAMPLE_PRT("cropH       : %d\n", pstChnCfg->stChnAttr.stCropRect.u32Height);

    return 0;
}

VIM_S32 SAMPLE_COMM_JPEG_ParseCfg(const char *cfg_filename, JPEG_CFG_S *pstJpegCfg)
{
    FILE *pFile = NULL;
    VIM_S32 VALUE = 0;
    VIM_S32 ChnCfgOffset = 0;
    VIM_S32 ChnCfgOffsetEnd = 0;

    if (cfg_filename)
    {
        pFile = fopen((void *)cfg_filename, "r");
        if (pFile == NULL)
        {
            return VIM_FAILURE;
        }
    }
    else
    {
        SAMPLE_PRT("failed, the input cfg file is NULL\n");
        return VIM_FAILURE;
    }

    if (WAVE_GetValue(pFile, "chnnum", &VALUE, 0))
    {
        pstJpegCfg->u32ChnNums = VALUE;
    }
    if (WAVE_GetValue(pFile, "useExtBufs", &VALUE, 0))
    {
        pstJpegCfg->stBufAttr.u32UseExtBufs = VALUE;
    }
    if (WAVE_GetValue(pFile, "numOutBufs", &VALUE, 0))
    {
        pstJpegCfg->stBufAttr.u32NumOutBufs = VALUE;
    }
    if (WAVE_GetValue(pFile, "sizeOutBufs", &VALUE, 0))
    {
        pstJpegCfg->stBufAttr.u32SizeOutBufs = VALUE;
    }

    SAMPLE_PRT("======== cfg dump ========\n");
    SAMPLE_PRT("chnnum      : %d\n", pstJpegCfg->u32ChnNums);
    SAMPLE_PRT("useExtBufs  : %d\n", pstJpegCfg->stBufAttr.u32UseExtBufs);
    SAMPLE_PRT("numOutBufs  : %d\n", pstJpegCfg->stBufAttr.u32NumOutBufs);
    SAMPLE_PRT("sizeOutBufs : %d\n\n", pstJpegCfg->stBufAttr.u32SizeOutBufs);

    for (unsigned int i = 0; i < pstJpegCfg->u32ChnNums; i++)
    {
        JPEG_CHN_CFG_S *pstChnCfg = &pstJpegCfg->stChnCfg[i];

        GetCfgOffset(pFile, "ChnStartFlag", (i + 1), &ChnCfgOffset);
        GetCfgOffset(pFile, "ChnEndFlag", (i + 1), &ChnCfgOffsetEnd);
        parseJpegCfg(pstChnCfg, (char *)cfg_filename, (VIM_U32)ChnCfgOffset, (VIM_U32)ChnCfgOffsetEnd);
        printJpegCfg(pstChnCfg);
        pstChnCfg->work = 1;
    }

    if (pFile)
    {
        fclose(pFile);
    }

    return VIM_SUCCESS;
}

/******************************************************************************
 * function : MPI jpeg alloc VideoBuffer
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_JPEG_Malloc(VIM_U32 u32Width,
										 VIM_U32 u32Height,
										 PIXEL_FORMAT_E enPixelFormat,
										 VIDEO_FRAME_INFO_S *pstFrame)
{
    VIM_S32 s32Ret;
    VIM_U32 width_16 =  ((u32Width + 15) / 16) * 16;
	 VIM_U32 width = CEILING_2_POWER(width_16, SAMPLE_SYS_ALIGN_WIDTH);
	VIM_U32 height = CEILING_2_POWER(u32Height, SAMPLE_SYS_ALIGN_WIDTH);
    VIM_U32 r0;
	VIM_U32 r1;
	VIM_U32 r2;

    r0 = width * height;
    r1 = r2 = 0;

    switch (enPixelFormat)
    {
    case PIXEL_FORMAT_YUV_PLANAR_420:
        r1 = r2 = r0 / 4;
        break;
    case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        r1 = r0 / 2;
        r2 = 0;
        break;
    case PIXEL_FORMAT_YUV_PLANAR_422:
        r1 = r2 = r0 / 2;
        break;
    case PIXEL_FORMAT_YUV_SEMIPLANAR_422:
        r1 = r0;
        r2 = 0;
        break;
    case PIXEL_FORMAT_YUV_PLANAR_444:
        r1 = r2 = r0;
        break;
    case PIXEL_FORMAT_YUV_SEMIPLANAR_444:
        r1 = 2 * r0;
        r2 = 0;
        break;
    default:
        ///< SAMPLE_PRT("as yuv400!\n");
        break;
    }

    memset(pstFrame, 0, sizeof(*pstFrame));
    s32Ret = VIM_MPI_JPEG_GetVB(pstFrame, r0 + r1 + r2);
    if (VIM_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("VIM_MPI_JPEG_GetVB failed!\n");
        return s32Ret;
    }
    width =u32Width;
    VIM_U32 viraddr = (VIM_U32)pstFrame->stVFrame.pVirAddr[0];
    VIM_U32 phyaddr = pstFrame->stVFrame.u32PhyAddr[0];

    pstFrame->stVFrame.u32Size[0] = r0;
    pstFrame->stVFrame.u32Size[1] = r1;
    pstFrame->stVFrame.u32Size[2] = r2;

    pstFrame->stVFrame.u32Stride[0] = width_16;
    pstFrame->stVFrame.enPixelFormat = enPixelFormat;
    pstFrame->stVFrame.u32Width = u32Width;
    pstFrame->stVFrame.u32Height = u32Height;

    if (r1)
    {
        pstFrame->stVFrame.pVirAddr[1] = (void *)(viraddr + r0);
        pstFrame->stVFrame.u32PhyAddr[1] = phyaddr + r0;
    }
    if (r2)
    {
        pstFrame->stVFrame.pVirAddr[2] = (void *)(viraddr + r0 + r1);
        pstFrame->stVFrame.u32PhyAddr[2] = phyaddr + r0 + r1;
    }

    return 0;
}

/******************************************************************************
 * function : MPI jpeg release VideoBuffer
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_JPEG_Free(VIDEO_FRAME_INFO_S *pstFrame)
{
    return VIM_MPI_JPEG_ReleaseVB(pstFrame, 0);
}

/******************************************************************************
 * function : crop alignment
 ******************************************************************************/
VIM_VOID SAMPLE_COMM_JPEG_AlignRect(RECT_S *rect,
											  VIM_S32 xa,
											  VIM_S32 ya,
											  VIM_S32 wa,
											  VIM_S32 ha)
{
    rect->s32X = rect->s32X / xa * xa;
    rect->s32Y = rect->s32Y / ya * ya;
    rect->u32Width = rect->u32Width / wa * wa;
    rect->u32Height = rect->u32Height / ha * ha;
}

VIM_VOID SAMPLE_COMM_JPEG_CheckRect(RECT_S *rect, PIXEL_FORMAT_E enPixFmt)
{
    switch (enPixFmt)
    {
    case PIXEL_FORMAT_YUV_PLANAR_420:
        SAMPLE_COMM_JPEG_AlignRect(rect, 16, 2, 16, 16);
        break;
    case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        SAMPLE_COMM_JPEG_AlignRect(rect, 8, 2, 16, 16);
        break;
    case PIXEL_FORMAT_YUV_PLANAR_422:
        SAMPLE_COMM_JPEG_AlignRect(rect, 16, 1, 16, 8);
        break;
    case PIXEL_FORMAT_YUV_SEMIPLANAR_422:
        SAMPLE_COMM_JPEG_AlignRect(rect, 8, 1, 16, 8);
        break;
    case PIXEL_FORMAT_YUV_PLANAR_444:
        SAMPLE_COMM_JPEG_AlignRect(rect, 8, 1, 8, 8);
        break;
    case PIXEL_FORMAT_YUV_SEMIPLANAR_444:
        SAMPLE_COMM_JPEG_AlignRect(rect, 8, 1, 8, 8);
        break;
    default:
        SAMPLE_PRT("%s invalid yuv format!\n", __func__);
        break;
    }
}

/******************************************************************************
 * function : MPI jpeg init
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_JPEG_Init(JPEG_BUF_ATTR_S *pstBufAttr)
{
    return VIM_MPI_JPEG_Init(pstBufAttr);
}

/******************************************************************************
 * function : MPI jpeg exit
 ******************************************************************************/
VIM_VOID SAMPLE_COMM_JPEG_Exit(void)
{
    VIM_MPI_JPEG_Exit();
    return;
}

VIM_U32 SAMPLE_COMM_JPEG_SaveStream(JPEG_CHN JpgChn, VENC_STREAM_S *pstStream)
{
    int err;
    char filename[64] = {0};
    static VIM_U32 frame[JPEGE_MAX_CHN_NUM] = {0};
    char folderName[] = "jpeg";

    if (access(folderName, 0) == -1)
    {
        mkdir(folderName, 0777);
    }
    snprintf(filename, sizeof(filename), "jpeg/ch%d-ID%04d-%04d.jpg",
                                         JpgChn,
                                         pstStream->yuvSeqNum,
                                         frame[JpgChn]);

    FILE *fp = fopen(filename, "wb");

    if (!fp)
    {
        SAMPLE_PRT("%s[chn%d]: open(%s) errno[%d]\n", __func__, JpgChn, filename, errno);
        return -1;
    }
    err = fwrite(pstStream->pstPack[0]->pu8Addr, pstStream->pstPack[0]->u32Len, 1, fp);
    if (err != 1)
    {
        SAMPLE_PRT("%s[chn%d]: write(%s) errno[%d]\n", __func__, JpgChn, filename, errno);
		err = fclose(fp);
	    if (err)
	    {
	        SAMPLE_PRT("%s[chn%d]: fclose(%s) errno[%d]\n", __func__, JpgChn, filename, errno);
	    }

        return -1;
    }

    err = fclose(fp);
    if (err)
    {
        SAMPLE_PRT("%s[chn%d]: fclose(%s) errno[%d]\n", __func__, JpgChn, filename, errno);
        return -1;
    }

    frame[JpgChn]++;
    return 0;
}

VIM_VOID *SAMPLE_COMM_JPEG_GetVencStreamProc(VIM_VOID *p)
{
    VIM_S32 i;
    VIM_S32 s32Ret;
    struct timeval TimeoutVal;
    VIM_S32 maxfd = 0;
    fd_set read_fds;
    VIM_S32 VencFd[JPEGE_MAX_CHN_NUM];
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    JPEG_VENC_GETSTREAM_PARA_S *pstPara = (JPEG_VENC_GETSTREAM_PARA_S *)p;
    VIM_S32 s32ChnTotal = pstPara->s32Cnt;

    /******************************************
     *step 1:  check & prepare save-file & venc-fd
     ******************************************/
    if (s32ChnTotal > JPEGE_MAX_CHN_NUM)
    {
        SAMPLE_PRT("input count invaild\n");
        return NULL;
    }

    for (i = 0; i < s32ChnTotal; i++)
    {
        /* Set Venc Fd. */
        VencFd[i] = VIM_MPI_JPEG_GetFd(i);
        if (VencFd[i] < 0)
        {
            SAMPLE_PRT("VIM_MPI_JPEG_GetFd failed with %#x!\n", VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i])
        {
            maxfd = VencFd[i];
        }
    }

    /******************************************
     *step 2:  Start to get streams of each channel.
     ******************************************/
    SAMPLE_PRT("Sample thread start ...\n");

    while (VIM_TRUE == pstPara->bThreadStart)
    {
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++)
        {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec = 5;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("select failed: %d!\n", errno);
            break;
        }
        else if (s32Ret == 0)
        {
            continue;
        }
        else
        {
            for (i = 0; i < s32ChnTotal; i++)
            {
                if (FD_ISSET(VencFd[i], &read_fds))
                {
                    /*******************************************************
                     *step 2.1 : query how many packs in one-frame stream.
                     *******************************************************/
                    memset(&stStream, 0, sizeof(stStream));
                    s32Ret = VIM_MPI_JPEG_Query(i, &stStat);
                    if (VIM_SUCCESS != s32Ret)
                    {
                        SAMPLE_PRT("VIM_MPI_JPEG_Query chn[%d] failed errno[%d]\n", i, errno);
                        break;
                    }

                    /*******************************************************
                     *step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames \
                     *at the same time,for example:
                     *if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
                     *{
                     *SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                     *continue;
                     *}
                     *******************************************************/
                    if (0 == stStat.u32CurPacks)
                    {
                        SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                    }
                    /*******************************************************
                     *step 2.3 : malloc corresponding number of pack nodes.
                     *******************************************************/
                    stStream.pstPack[0] =
                        (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack[0])
                    {
                        SAMPLE_PRT("malloc stream pack failed!\n");
                        break;
                    }

                    /*******************************************************
                     *step 2.4 : call mpi to get one-frame stream
                     *******************************************************/
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = VIM_MPI_JPEG_GetStream(i, &stStream, 0);
                    if (VIM_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack[0]);
                        stStream.pstPack[0] = NULL;
                        SAMPLE_PRT("VIM_MPI_JPEG_GetStream failed with %#x!\n", s32Ret);
                        break;
                    }

                    /*******************************************************
                     *step 2.5 : save frame to file
                     *******************************************************/
                    s32Ret = SAMPLE_COMM_JPEG_SaveStream(i, &stStream);
                    if (VIM_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack[0]);
                        stStream.pstPack[0] = NULL;
                        break;
                    }

                    /*******************************************************
                     *step 2.6 : release stream
                     *******************************************************/
                    s32Ret = VIM_MPI_JPEG_ReleaseStream(i, &stStream);
                    if (VIM_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack[0]);
                        stStream.pstPack[0] = NULL;
                        SAMPLE_PRT("VIM_MPI_JPEG_ReleaseStream failed with %#x!\n", s32Ret);
                        break;
                    }
                    /*******************************************************
                     *step 2.7 : free pack nodes
                     *******************************************************/
                    free(stStream.pstPack[0]);
                    stStream.pstPack[0] = NULL;
                }
            }
        }
    }

    SAMPLE_PRT("Sample thread stoped\n");

    return (void *)0;
}

/******************************************************************************
 * funciton : Start venc stream mode (h264, mjpeg)
 * note      : rate control parameter need adjust, according your case.
 ******************************************************************************/
#define RECT_CLEAN(r) ((r).s32X + (r).s32Y + (r).u32Width + (r).u32Height == 0)

VIM_S32 SAMPLE_COMM_JPEG_Start(JPEG_CHN JpgChn, JPEG_CHN_ATTR_S *pstAttr)
{
    VIM_S32 s32Ret;

    /******************************************
     *step 1:  Create JPEG Channel
     ******************************************/
    s32Ret = VIM_MPI_JPEG_CreateChn(JpgChn, pstAttr);
    if (VIM_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("VIM_MPI_JPEG_CreateChn [%d] faild with %#x!\n", JpgChn, s32Ret);
        return s32Ret;
    }

    /******************************************
     *step 2:  Start Recv Venc Pictures
     ******************************************/
    s32Ret = VIM_MPI_JPEG_StartRecvPic(JpgChn);
    if (VIM_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("VIM_MPI_JPEG_StartRecvPic faild with%#x!\n", s32Ret);
        return VIM_FAILURE;
    }

    return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : send frame to jpeg codec
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_JPEG_SendFrame(JPEG_CHN JpgChn, VIDEO_FRAME_INFO_S *pstFrame)
{
    return VIM_MPI_JPEG_SendFrame(JpgChn, pstFrame);
}

/******************************************************************************
 * funciton : Stop venc ( stream mode -- JPEG )
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_JPEG_Stop(JPEG_CHN JpgChn)
{
    VIM_S32 s32Ret;
    /******************************************
     *step 1:  Stop Recv Pictures
     ******************************************/
    s32Ret = VIM_MPI_JPEG_StopRecvPic(JpgChn);
    if (VIM_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("VIM_MPI_JPEG_StopRecvPic chn[%d] failed with %#x!\n", JpgChn, s32Ret);
        return VIM_FAILURE;
    }
    /******************************************
     *step 2:  Distroy Venc Channel
     ******************************************/
    s32Ret = VIM_MPI_JPEG_DestroyChn(JpgChn);
    if (VIM_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("VIM_MPI_JPEG_DestroyChn chn[%d] failed with %#x!\n", JpgChn, s32Ret);
        return VIM_FAILURE;
    }
    return VIM_SUCCESS;
}

/******************************************************************************
 * funciton : start get venc stream process thread
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_JPEG_StartGetStream(VIM_S32 s32Cnt)
{
    gs_stPara.bThreadStart = VIM_TRUE;
    gs_stPara.s32Cnt = s32Cnt;

    return pthread_create(&gs_VencPid, 0, SAMPLE_COMM_JPEG_GetVencStreamProc, (VIM_VOID *)&gs_stPara);
}
/******************************************************************************
 * funciton : stop get venc stream process.
 ******************************************************************************/
VIM_S32 SAMPLE_COMM_JPEG_StopGetStream(void)
{
    if (VIM_TRUE == gs_stPara.bThreadStart)
    {
        gs_stPara.bThreadStart = VIM_FALSE;
        pthread_join(gs_VencPid, 0);
    }
    return VIM_SUCCESS;
}

VIM_S32 SAMPLE_COMM_JPEG_GetStreamBlock(JPEG_CHN JpgChn, VIM_S32 s32MilliSec, VIM_U32 save)
{
    VIM_S32 s32Ret;
    VENC_PACK_S pack = {0};
    JPEG_STREAM_S stStream = {.pstPack[0] = &pack};

    s32Ret = VIM_MPI_JPEG_GetStream(JpgChn, &stStream, s32MilliSec);
    if (VIM_SUCCESS == s32Ret)
    {
        if (save)
        {
             SAMPLE_COMM_JPEG_SaveStream(JpgChn, &stStream);
        }

        VIM_MPI_JPEG_ReleaseStream(JpgChn, &stStream);

        return 0;
    }
    else
    {
        SAMPLE_PRT("%s chn[%d] failed with %#x!\n", __func__, JpgChn, s32Ret);
        return -1;
    }
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
