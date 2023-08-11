#include "sample_comm_vdec.h"

static int flag_end_decode = 0;
static SAMPLE_VDEC_CFG testParam[MAX_VDEC_CHN];

#define BUFF_STRIDE_TO_PICTURE_STRIDE(buffStride, pixelFormat) \
    (   pixelFormat==PIXEL_FORMAT_YUV_PLANAR_420_P10 || pixelFormat==PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10? buffStride/4*3:  \
        pixelFormat==PIXEL_FORMAT_YUV_PLANAR_420_P10_2BYTE_LSB || pixelFormat==PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10_2BYTE_LSB? buffStride/2: \
        buffStride )

VIM_VOID SAMPLE_COMM_VDEC_SetEndFlag(VIM_S32 endFlag)
{
    flag_end_decode = endFlag;
}

VIM_S32 SAMPLE_COMM_VDEC_GetEndFlag()
{
    return flag_end_decode;
}

SAMPLE_VDEC_CFG* SAMPLE_COMM_VDEC_GetTestParam(VIM_S32 index)
{
    if (index < MAX_VDEC_CHN)
        return &testParam[index];
    else
        return VIM_NULL;
}

/*  [说明] 使用默认配置vdecAttr来初始化VPU
    [参数] ...
    [返回值] VIM_SUCCESS 成功，其他失败     */
VIM_S32 SAMPLE_COMM_VDEC_Init()
{
    VIM_S32 ret = VIM_FAILURE;
    VDEC_ATTR_CONFIG_S vdecAttr = {8, 1, 4096, 2304, 10, 30, VIM_NULL, {0}};
    ret = VIM_MPI_VDEC_Init(&vdecAttr);
    if (ret != VIM_SUCCESS) {
        VDEC_SAMPLE_ERR("Failed to Init VPU : %x",ret);
    }
    memset(testParam, 0, sizeof(SAMPLE_VDEC_CFG)*MAX_VDEC_CHN);
    return ret;
};

/*  [说明] 退出VPU 
    [参数] ...
    [返回值] VIM_SUCCESS 成功，其他失败     */
VIM_S32 SAMPLE_COMM_VDEC_Exit()
{
    VIM_S32 ret = VIM_FAILURE;
    ret = VIM_MPI_VDEC_Exit();
    if (ret != VIM_SUCCESS) {
        VDEC_SAMPLE_ERR("Failed to Exit VPU : %x",ret);
    }
    return ret;
}

VIM_VOID SAMPLE_COMM_VDEC_GetOneNaluFromFile(FILE *codingFile, VIM_U8 *pu8Addr, SAMPLE_VDEC_NALU_INFO *pnalu_info, PAYLOAD_TYPE_E codeType)
{
    VIM_U8 currentStage = 0; // 0-3: 发现startcode(00 00 00 01); 4: 解析类型; 9: 结束查找
    VIM_U8 flag_found_start = 0; // 发现第一个startcode
    VIM_U32 nalu_len = 0;
    do {
        if (fread(&pu8Addr[nalu_len], 1, 1, codingFile) <= 0)
            break;
        switch (currentStage) {
            case 0:
            case 1:
            case 2:
                /*// 测试在帧间插入数据‘0x00’; x 帧号; flag_found_start 仅在slice中插入; nalu_len 插入位置;
                if (testParam[0].u32FeedIndex == 30 && flag_found_start == 1 && nalu_len%510 == 0) {
                    nalu_len ++;
                    memcpy(&pu8Addr[nalu_len], "\x00\x00\x00\x00\x00\x01", 6);
                    nalu_len += 5;
                    continue;
                } */
                if (pu8Addr[nalu_len] != 0x00) {
                    currentStage = 0;
                } else
                    currentStage ++;                // [1-3]*0x00
                break;
            case 3:
                if (pu8Addr[nalu_len] == 0x00)    // [3-N]*0x00
                    break;
                if (pu8Addr[nalu_len] != 0x01) {   // [3-N]*0x00 not followed by 0x01: bad startcode
                    currentStage = 0;
                    break;
                }
                if (flag_found_start) {
                    nalu_len -= 4;
                    currentStage = 9;
                    fseek(codingFile, -4, SEEK_CUR);
                } else
                    currentStage = 4;
                break;
            case 4:
                if (codeType == PT_SVAC2)
                    pnalu_info->nalu_type = (pu8Addr[nalu_len] & 0x3C) >> 2;
                else
                    pnalu_info->nalu_type = pu8Addr[nalu_len] >> 1;
                currentStage = 0;
                flag_found_start = 1;
                break;
            default:
                currentStage = 0;
                break;
        }
        nalu_len ++;
    } while (currentStage != 9 && !flag_end_decode);
    pnalu_info->total_size = nalu_len;
}

/*  [*注意*] 为方便解析编码码流，函数中按Byte读取数据，对于大码率文件此方法效率低下，在测试最大解码速率时不应使用此方法 
    [说明] 从编码文件中读取一帧， 当 frame_len == 0 表示读至文件末尾
    [参数] codingFile: 编码文件;  pu8Addr: 存放帧数据地址;  codeType: 编码类型
    [返回值] 读取到一帧的长度   */
VIM_U32 SAMPLE_COMM_VDEC_GetOneFrameFromFile(FILE *codingFile, VIM_U8 *pu8Addr, PAYLOAD_TYPE_E codeType)
{
    VIM_U32 frame_len = 0;
    SAMPLE_VDEC_NALU_INFO nalu_info;
    do {
        memset(&nalu_info, 0, sizeof(SAMPLE_VDEC_NALU_INFO));
        SAMPLE_COMM_VDEC_GetOneNaluFromFile(codingFile, pu8Addr + frame_len, &nalu_info, codeType);
        frame_len += nalu_info.total_size;
        if (frame_len == 0)
            break;
        switch (nalu_info.nalu_type) {
    #ifdef SVAC2_PARSE_EL_AND_BL_AS_ONE_FRAME
            case SAMPLE_VDEC_NALU_TYPE_SVAC_ISLICE_EL:
            case SAMPLE_VDEC_NALU_TYPE_SVAC_NON_ISLICE_EL:
    #else
            case SAMPLE_VDEC_NALU_TYPE_HEVC_TRAIL_N:
            case SAMPLE_VDEC_NALU_TYPE_HEVC_TRAIL_R: // case SAMPLE_VDEC_NALU_TYPE_SVAC_NON_ISLICE:
            case SAMPLE_VDEC_NALU_TYPE_SVAC_ISLICE:
    #endif
            case SAMPLE_VDEC_NALU_TYPE_HEVC_IDR_W_RADL:
            case SAMPLE_VDEC_NALU_TYPE_HEVC_IDR_N_LP:
            case SAMPLE_VDEC_NALU_TYPE_HEVC_CRA_NUT:
                // printf("frame size = %d\n", frame_len);
                return frame_len; 
            // 测试丢弃 SPS 或 PPS 信息
            // case SAMPLE_VDEC_NALU_TYPE_HEVC_SPS_NUT:
            // case SAMPLE_VDEC_NALU_TYPE_SVAC_SPS:
            case SAMPLE_VDEC_NALU_TYPE_HEVC_PPS_NUT:
            case SAMPLE_VDEC_NALU_TYPE_SVAC_PPS:
                // printf("drop pps size = %d\n", nalu_info.total_size);
                // frame_len -= nalu_info.total_size;
                break;
            default:
                break;
        }
        // printf("nalu type = %2d, nalu size = %d\n", nalu_info.nalu_type, nalu_info.total_size);
    } while (nalu_info.total_size);
    return frame_len;
}

VIM_S32 SAMPLE_COMM_VDEC_FeedStream(SAMPLE_VDEC_CFG *chnTestParam, VDEC_STREAM_S *sVdecStream, VIM_U8 flag_retry)
{
    VIM_S32 ret = VIM_FAILURE;
    VDEC_CHN_STAT_S chnStat;
    // printf("chn %d feed: %d, size: %d\n", chnTestParam->VdecChn, chnTestParam->u32FeedIndex, chnTestParam->vdecStream.u32Len);
    do {
        // // 测试 每 5帧 丢弃 1帧 数据
        // if (chnTestParam->u32FeedIndex % 5 == 1)
        // {
        //     chnTestParam->u32FeedIndex ++;
        //     goto retry;
        // }
        ret = VIM_MPI_VDEC_Query(chnTestParam->VdecChn, &chnStat);
        if (ret != VIM_SUCCESS || chnStat.u32LeftStreamBytes > chnTestParam->chnAttr.u32StreamSize/3*2) {
            usleep(10000);
            continue;
        }
        ret = VIM_MPI_VDEC_FeedingStream(chnTestParam->VdecChn, sVdecStream, 0);
        if (ret == VIM_SUCCESS) {
            if (sVdecStream->u32Len) {
                chnTestParam->u32FeedIndex ++;
            }
            break;
        } else if (ret == VIM_ERR_VDEC_NOBUF || ret == VIM_ERR_VDEC_TIMEOUT || ret == VIM_ERR_VDEC_CHN_PAUSED || ret == VIM_ERR_VDEC_BUF_FULL) {
            usleep(1000);
            if (flag_retry)
                continue;
            break;
        } else {
            if (chnTestParam->codingFile)
                rewind(chnTestParam->codingFile);
            sVdecStream->bEndofStream = VIM_TRUE;
            VDEC_SAMPLE_ERR("feedStream error, ret = 0x%X, chn = %d", ret, chnTestParam->VdecChn);
            break;
        }
    } while (!flag_end_decode);
    return ret;
}

/*  [说明] 读取文件并送入解码器，通常作为线程来执行
    [参数] ...
    [返回值] VIM_SUCCESS 成功，其他失败     */
VIM_U32 SAMPLE_COMM_VDEC_FEEDING_STREAM(SAMPLE_VDEC_CFG* chnTestParam)
{
    VIM_S32 ret = 0;
    VIM_CHAR threadname[16];
    VDEC_STREAM_S sVdecStream;
    memset(threadname, 0, 16);
    sprintf(threadname, "chn%d_feed", chnTestParam->VdecChn);
#ifdef __USE_GNU
    pthread_setname_np(chnTestParam->thread_feed, threadname);
#else
    prctl(PR_SET_NAME, threadname);
#endif
    chnTestParam->codingFile = fopen(chnTestParam->inputFilepath, "rb");
    if (!chnTestParam->codingFile) {
        VDEC_SAMPLE_ERR("open file %s failed", chnTestParam->inputFilepath);
        return VIM_FAILURE;
    }
    memset(&sVdecStream, 0, sizeof(VDEC_STREAM_S));
    sVdecStream.pu8Addr = malloc(1024*1024*6);
    if (!sVdecStream.pu8Addr) {
        VDEC_SAMPLE_ERR("failed to alloc mem for sVdecStream.pu8Addr\n");
        return VIM_FAILURE;
    }
    memset(sVdecStream.pu8Addr, 0, 1024*1024*6); 
    do {
        if (chnTestParam->chnAttr.enFeedingMode == FEEDING_MODE_FIXED_SIZE)
            ret = fread(sVdecStream.pu8Addr, 1, FEEDING_SIZE, chnTestParam->codingFile);
        else
            ret = SAMPLE_COMM_VDEC_GetOneFrameFromFile(chnTestParam->codingFile, sVdecStream.pu8Addr, chnTestParam->chnAttr.enCodType);
        if (ret == 0) {
            if (--chnTestParam->feedLoopCount > 0) { 
                rewind(chnTestParam->codingFile);
                continue;
            } else {
                sVdecStream.bEndofFrame = VIM_TRUE;
                sVdecStream.bEndofStream = VIM_TRUE;
            }
        }
        sVdecStream.u64PTS = time(VIM_NULL);
        sVdecStream.u32Len = ret;
        ret = SAMPLE_COMM_VDEC_FeedStream(chnTestParam, &sVdecStream, VIM_TRUE);
    } while (!sVdecStream.bEndofStream && ret == VIM_SUCCESS && !flag_end_decode);
    // if (chnTestParam->u32FeedIndex < 2)
    //     chnTestParam->chnStates = SAMPLE_VDEC_CHN_END;
    if (sVdecStream.pu8Addr)
        free(sVdecStream.pu8Addr);
    if (chnTestParam->codingFile)
        fclose(chnTestParam->codingFile);
    chnTestParam->codingFile = VIM_NULL;
    VDEC_SAMPLE_DEBUG("feeding thread return, chn = %d", chnTestParam->VdecChn);
    return VIM_SUCCESS;
}

/*  [说明] 输出结果至本地YUV文件
    [参数] VdecChn 解码通道; pFile YUV文件描述符; pstSeqHeader 当前序列帧的Header信息，通过之前步骤StartRecvStream取得; sPack 解码输出包; resize 如果有效，则以resize作为输出大小
    [返回值] VIM_SUCCESS 成功，其他失败     */
static VIM_S32 SAMPLE_VDEC_Dump2Yuv(VDEC_CHN VdecChn ,FILE* pFile, VDEC_SEQHEADER_S *pstSeqHeader, VDEC_PACK_S *sPack, RECT_S* resize)
{
    size_t writeCount;
    VIM_U32 height = sPack->sFrame.stVFrame.u32Height;
    VIM_U32 width = sPack->sFrame.stVFrame.u32Width;
    VIM_U32 buff_stride = pstSeqHeader->u32BuffStride;
    VIM_U32 linenum = 0, ycount = 0, uvcount = 0;
    if (pFile == VIM_NULL || sPack->sFrame.stVFrame.u32PhyAddr[0] == 0 || sPack->sFrame.stVFrame.pVirAddr[0] == 0)
        return VIM_FAILURE;
    if ( resize->u32Height > 0 && resize->u32Width > 0 && resize->u32Width <= width && resize->u32Height <= height ) {
        // 保存裁减图像
        switch (sPack->sFrame.stVFrame.enPixelFormat) {
            case PIXEL_FORMAT_YUV_PLANAR_420:
            case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
                if (resize->u32Width%2) resize->u32Width = resize->u32Width/2*2;
                if (resize->u32Height%2) resize->u32Height= resize->u32Height/2*2;
                for (linenum = 0; linenum < resize->u32Height; linenum ++) {
                    fwrite(sPack->sFrame.stVFrame.pVirAddr[0] + buff_stride * linenum, resize->u32Width, 1, pFile);
                    ycount += resize->u32Width;
                }
                if (sPack->sFrame.stVFrame.enPixelFormat == PIXEL_FORMAT_YUV_PLANAR_420) {
                    for (linenum = 0; linenum < resize->u32Height/4; linenum++) {
                        uvcount += fwrite(sPack->sFrame.stVFrame.pVirAddr[1] + buff_stride * linenum, resize->u32Width/2, 1, pFile);
                        uvcount += fwrite(sPack->sFrame.stVFrame.pVirAddr[1] + buff_stride * linenum + buff_stride/2, resize->u32Width/2, 1, pFile);
                    }
                    for (linenum = 0; linenum < resize->u32Height/4; linenum++) {
                        uvcount += fwrite(sPack->sFrame.stVFrame.pVirAddr[2] + buff_stride * linenum, resize->u32Width/2, 1, pFile);
                        uvcount += fwrite(sPack->sFrame.stVFrame.pVirAddr[2] + buff_stride * linenum + buff_stride/2, resize->u32Width/2, 1, pFile);
                    }
                } else {
                    for (linenum = 0; linenum < resize->u32Height/2; linenum++) {
                        uvcount += fwrite(sPack->sFrame.stVFrame.pVirAddr[1] + buff_stride * linenum, resize->u32Width, 1, pFile);
                    }
                }
                VDEC_SAMPLE_DEBUG("chn%d, Save Frame %d(%dx%d)" ,
                        VdecChn ,sPack->sFrame.stVFrame.u32TimeRef, resize->u32Width, resize->u32Height);
                break;
            default:
                goto SAVE_ORIGIN_FRAME;
        }
    } else {
        // 保存原始图像
SAVE_ORIGIN_FRAME:
        // writeCount = fwrite(sPack->sFrame.stVFrame.pVirAddr[0], 1, sPack->sFrame.stVFrame.u32Size[0] + sPack->sFrame.stVFrame.u32Size[1] + sPack->sFrame.stVFrame.u32Size[2], pFile);
        writeCount = fwrite(sPack->sFrame.stVFrame.pVirAddr[0], 1, sPack->sFrame.stVFrame.u32Size[0], pFile);
        writeCount += fwrite(sPack->sFrame.stVFrame.pVirAddr[1], 1, sPack->sFrame.stVFrame.u32Size[1], pFile);
        writeCount += fwrite(sPack->sFrame.stVFrame.pVirAddr[2], 1, sPack->sFrame.stVFrame.u32Size[2], pFile);
        VDEC_SAMPLE_DEBUG("chn%d, Save Frame %d(%dx%d): framesize = %d bytes, and write %d bytes" ,
                        VdecChn ,sPack->sFrame.stVFrame.u32TimeRef, 
                        BUFF_STRIDE_TO_PICTURE_STRIDE(sPack->sFrame.stVFrame.u32Stride[0], sPack->sFrame.stVFrame.enPixelFormat), 
                        sPack->sFrame.stVFrame.u32Height, 
                        sPack->sFrame.stVFrame.u32Size[0] + sPack->sFrame.stVFrame.u32Size[1] + sPack->sFrame.stVFrame.u32Size[2], 
                        writeCount );
    }
    return VIM_SUCCESS;
}

/*  [说明] 在不使用绑定功能时, 使用此方法循环获取解码结果, 通常作为线程来执行
    [参数] ...
    [返回值] ...                            */
VIM_S32 SAMPLE_COMM_VDEC_TEST_DECODER(SAMPLE_VDEC_CFG *chnTestParam)
{
    VIM_S32 ret = 0;
    VDEC_PACK_S sPack;
    FILE* yuvFile = VIM_NULL;
    FILE* yuvFile_bl = VIM_NULL;
    VIM_CHAR *expectStreamType = chnTestParam->chnAttr.enCodType == PT_SVAC2? "SVAC2": "H265";
    VIM_CHAR threadname[16] = {0 ,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    sprintf(threadname, "chn%d_sample", chnTestParam->VdecChn & 0xf);
#ifdef __USE_GNU
    pthread_setname_np(chnTestParam->thread_dec, threadname);
#else
    prctl(PR_SET_NAME, threadname);
#endif
    memset(&sPack, 0, sizeof(VDEC_PACK_S));
SPS_CHANGE:
    memset(&chnTestParam->seqHeader, 0, sizeof(VDEC_SEQHEADER_S));
    do
    {
        if (flag_end_decode) {
            VDEC_SAMPLE_DEBUG("chn%d, cancel start decode", chnTestParam->VdecChn); 
            break;
        }
        ret = VIM_MPI_VDEC_GetSeqHeader(chnTestParam->VdecChn, &chnTestParam->seqHeader);
        switch (ret) {
            case VIM_SUCCESS:
                VDEC_SAMPLE_DEBUG("chn%d started! StreamType = %s, PicWidth = %d, PicHeight = %d, Stride = %d, BuffHeight = %d\n", 
                                    chnTestParam->VdecChn, expectStreamType,
                                    chnTestParam->seqHeader.u32PicWidth, chnTestParam->seqHeader.u32PicHeight,
                                    BUFF_STRIDE_TO_PICTURE_STRIDE(chnTestParam->seqHeader.u32BuffStride, chnTestParam->chnAttr.enPixelFormat),
                                    chnTestParam->seqHeader.u32BuffHeight);
                break;
            case VIM_ERR_VDEC_NOT_STARTED:
                // VDEC_SAMPLE_DEBUG("chn%d, not started!", chnTestParam->VdecChn);
                break;
            case VIM_ERR_VDEC_NOT_SUPPORT:
                VDEC_SAMPLE_ERR("chn%d, codec format or resolution not support!", chnTestParam->VdecChn);
                chnTestParam->chnStates = SAMPLE_VDEC_CHN_END;
                break; 
            default:
                break;
        }
        usleep(100*1000);
    } while (ret && chnTestParam->chnStates != SAMPLE_VDEC_CHN_END);
    if (chnTestParam->outputFilepath[0] != 0) {
        if (chnTestParam->seqHeader.u32SpatialSvcEnable) {
            VIM_CHAR filepath_bl[MAX_PATH_LENGTH+16];
            VIM_CHAR filepath_el[MAX_PATH_LENGTH+16];
            memset(filepath_bl,0,MAX_PATH_LENGTH+16);
            memset(filepath_el,0,MAX_PATH_LENGTH+16);
            VIM_U16 numerator = 1, denomirator = 1;
            if (chnTestParam->seqHeader.u32SpatialSvcEnable) {
                if (chnTestParam->seqHeader.u32SpatialSvcRation == 0) {
                    numerator = 3; 
                    denomirator = 4;
                } else {
                    denomirator = 2 * chnTestParam->seqHeader.u32SpatialSvcRation;
                }
            }
            sprintf(filepath_bl,"chn%d_bl_%dx%d_%s", chnTestParam->VdecChn,
                chnTestParam->seqHeader.u32PicWidth * numerator / denomirator,  
                chnTestParam->seqHeader.u32PicHeight * numerator / denomirator,
                chnTestParam->outputFilepath);
            sprintf(filepath_el,"chn%d_el_%dx%d_%s", chnTestParam->VdecChn,
                chnTestParam->seqHeader.u32PicWidth, chnTestParam->seqHeader.u32PicHeight,
                chnTestParam->outputFilepath);
            yuvFile = fopen(filepath_el, "wb");
            if (yuvFile == VIM_NULL) {
                VDEC_SAMPLE_ERR("failed to open %s", filepath_el);
                ret = -1;
                goto cleanup;
            }
            yuvFile_bl = fopen(filepath_bl, "wb");
            if (yuvFile_bl == VIM_NULL) {
                VDEC_SAMPLE_ERR("failed to open %s", filepath_bl);
                ret = -1;
                goto cleanup;
            }
        } else {
            VIM_CHAR filepath[MAX_PATH_LENGTH+16];
            memset(filepath,0,MAX_PATH_LENGTH+16);
            sprintf(filepath,"chn%d_%dx%d_%s", chnTestParam->VdecChn, chnTestParam->seqHeader.u32PicWidth, chnTestParam->seqHeader.u32PicHeight, chnTestParam->outputFilepath);
            yuvFile = fopen(filepath, "wb");
            if (yuvFile == VIM_NULL) {
                VDEC_SAMPLE_ERR("failed to open %s", chnTestParam->outputFilepath);
                ret = -1;
                goto cleanup;
            }
        }
    }
    // 获取解码后的帧，直到码流结束或者错误发生
    do {
        // 获取解码数据
        ret = VIM_MPI_VDEC_GetFrame(chnTestParam->VdecChn, &sPack, chnTestParam->blockFlag_GetFrame);
        switch (ret) {
            case VIM_SUCCESS:
                // 解码成功后的一些操作
                chnTestParam->u32FrameIndex ++;
                if (sPack.sFrameEx.u32SpatialSvcFlag && sPack.sFrameEx.u32SpatialSvcLayer == 1) {
                    chnTestParam->u32FrameIndex --;
                }
                if (chnTestParam->savePicWidth) {
                    chnTestParam->saveResize.s32X = 0;
                    chnTestParam->saveResize.s32Y = 0;
                    chnTestParam->saveResize.u32Width = sPack.sFrame.stVFrame.u32Width;
                    chnTestParam->saveResize.u32Height = sPack.sFrame.stVFrame.u32Height;
                }
                if (yuvFile) {
                    SAMPLE_VDEC_Dump2Yuv(chnTestParam->VdecChn, sPack.sFrameEx.u32SpatialSvcLayer == 0 && sPack.sFrameEx.u32SpatialSvcFlag == 1? yuvFile_bl: yuvFile, 
                        &chnTestParam->seqHeader, &sPack, &chnTestParam->saveResize);
                } else if (chnTestParam->captureFlag) {
                    char yuvname[64];
                    memset(yuvname ,0 ,64);
                    sprintf(yuvname, "chn%d_capture_%d.yuv", chnTestParam->VdecChn, sPack.sFrame.stVFrame.u32TimeRef);
                    FILE *tmpyuv = fopen(yuvname,"wb");
                    SAMPLE_VDEC_Dump2Yuv(chnTestParam->VdecChn, tmpyuv, &chnTestParam->seqHeader, &sPack, &chnTestParam->saveResize);
                    fclose(tmpyuv);
                    chnTestParam->captureFlag = 0;
                }
                if (chnTestParam->output_func) {
                    chnTestParam->output_func(chnTestParam->VdecChn, &sPack);
                }
                // 释放解码数据
                if (ret == VIM_SUCCESS) {
                    ret = VIM_MPI_VDEC_ReleaseFrame(chnTestParam->VdecChn,&sPack);
                    if (ret) {
                        VDEC_SAMPLE_DEBUG("ERR release frame encounter error: %x",ret);
                    } else if (sPack.sFrame.stVFrame.u32TimeRef % SAMPLE_VDEC_FPS_CALC_UNIT == 0) {
                    }
                }
                break;
            case VIM_ERR_VDEC_STREAM_INSUFFICIENT:
            case VIM_ERR_VDEC_FRAME_CACHED:
                break;
            case VIM_ERR_VDEC_NOT_STARTED:
            case VIM_ERR_VDEC_CHN_PAUSED:
                sleep(1);
                break;
            case VIM_ERR_VDEC_BUSY:
            case VIM_ERR_VDEC_NOBUF:
            case VIM_ERR_VDEC_TIMEOUT:
            case VIM_ERR_VDEC_BUF_FULL:
                break;
            case VIM_ERR_VDEC_STREAM_CORRUPTED:
                chnTestParam->u32BadFrameCount ++;
                break;
            case VIM_ERR_VDEC_UNKNOWN:
                sleep(1);
                break;
            case VIM_ERR_VDEC_SEQUENCE_CHANGED:
                if (yuvFile_bl) {
                    fclose(yuvFile_bl);
                    yuvFile_bl = VIM_NULL;
                }
                if (yuvFile) {
                    fclose(yuvFile);
                    yuvFile = VIM_NULL;
                }
                goto SPS_CHANGE; 
            default:
                VDEC_SAMPLE_DEBUG("chn%d GetFrame ret = %x, sPack.no = %d, test.FrameIdx = %d", chnTestParam->VdecChn, ret, sPack.sFrame.stVFrame.u32TimeRef, chnTestParam->u32FrameIndex);
                usleep(100000);
                break;
        }
        if (!chnTestParam->codingFile) {
            VDEC_CHN_STAT_S stStat;
            ret = VIM_MPI_VDEC_Query(chnTestParam->VdecChn, &stStat);
            if (chnTestParam->chnAttr.enFeedingMode) {
                if ((chnTestParam->u32FrameIndex + chnTestParam->u32BadFrameCount >= chnTestParam->u32FeedIndex && stStat.u32LeftStreamBytes == 0)) {
                    chnTestParam->chnStates = SAMPLE_VDEC_CHN_END;
                }
            } else {
                if (ret == VIM_SUCCESS || ret == VIM_ERR_VDEC_STREAM_INSUFFICIENT) {
                    if (stStat.u32LeftStreamBytes == 0 && stStat.u32LeftStreamFrames == 0)
                        chnTestParam->chnStates = SAMPLE_VDEC_CHN_END;
                }
            }
        }
    } while (chnTestParam->chnStates < SAMPLE_VDEC_CHN_END && !flag_end_decode);
cleanup:
    if (yuvFile)
        fclose(yuvFile);
    if (yuvFile_bl)
        fclose(yuvFile_bl);
    return ret;
}

VIM_S32 SAMPLE_COMM_VDEC_START_FEEDING(SAMPLE_VDEC_CFG *chnTestParam)
{
    return pthread_create(&chnTestParam->thread_feed, NULL,  ((chnTestParam->feed_func == VIM_NULL)? (VIM_VOID*)SAMPLE_COMM_VDEC_FEEDING_STREAM: chnTestParam->feed_func), chnTestParam);
}

VIM_S32 SAMPLE_COMM_USB_START_FEEDING(SAMPLE_VDEC_CFG *chnTestParam, VDEC_CHN VdecChn)
{
    VDEC_SAMPLE_DEBUG("%s chn%d enter.", __FUNCTION__, VdecChn);
    if (VdecChn == 0) {
        VDEC_SAMPLE_DEBUG("Create a USB feed thread!!!");
        return pthread_create(&chnTestParam->thread_feed, NULL, (VIM_VOID*)chnTestParam->usb_feedfunc, NULL);
    }
    return VIM_SUCCESS;
}

/*  [说明] 首先向通道喂一帧数据，然后使通道开始解码，并启动喂流线程
    [参数] ...
    [返回值] VIM_SUCCESS 成功； 其它失败    */
VIM_S32 SAMPLE_COMM_VDEC_START(SAMPLE_VDEC_CFG *chnTestParam, VDEC_CHN VdecChn)
{
    VIM_S32 ret = VIM_SUCCESS;
    if (!chnTestParam->blockFlag_GetFrame)
        chnTestParam->blockFlag_GetFrame = 300;
    if (!chnTestParam->blockFlag_StartRecv)
        chnTestParam->blockFlag_StartRecv = 3000;

    if (testParam[0].usb_feedfunc != VIM_NULL) {
        ret = SAMPLE_COMM_USB_START_FEEDING(chnTestParam, VdecChn);
        if (ret != VIM_SUCCESS)
            return ret;
        ret = SAMPLE_COMM_VDEC_START_RECV(chnTestParam);
        if (ret != VIM_SUCCESS)
            return ret;
    } else {
        ret = SAMPLE_COMM_VDEC_START_RECV(chnTestParam);
        if (ret != VIM_SUCCESS)
            return ret;
        ret = SAMPLE_COMM_VDEC_START_FEEDING(chnTestParam);
        if (ret != VIM_SUCCESS)
            return ret;
    }

    chnTestParam->chnStates = SAMPLE_VDEC_CHN_STARTED;
    return ret;
}

/*  [说明] 创建通道
    [参数] ...
    [返回值] VIM_SUCCESS 成功； 其它失败    */
VIM_S32 SAMPLE_COMM_VDEC_Create(SAMPLE_VDEC_CFG *chnTestParam)
{
    VIM_S32 ret = VIM_FAILURE;
    chnTestParam->chnStates = SAMPLE_VDEC_CHN_NONE;
    chnTestParam->chnAttr.u32BSBufferThreshold_H = chnTestParam->chnAttr.u32StreamSize / 4 * 3;
    chnTestParam->chnAttr.u32BSBufferThreshold_L = chnTestParam->chnAttr.u32StreamSize / 4;
    ret = VIM_MPI_VDEC_CreateChn(chnTestParam->VdecChn, &chnTestParam->chnAttr);
    if (VIM_SUCCESS != ret) {
        VDEC_SAMPLE_ERR("CreateChn %d failed, ret = 0x%X", chnTestParam->VdecChn, ret);
        return ret;
    }
    chnTestParam->chnStates = SAMPLE_VDEC_CHN_CREATED;
    return ret;
}

/*  [说明] 创建 FeedingStream 线程， 同时执行 StartRecvStream 方法， 此方法执行后， 通道已处于可绑定状态
    [参数] ...
    [返回值] VIM_SUCCESS 成功； 其它失败    */
VIM_S32 SAMPLE_COMM_VDEC_START_RECV(SAMPLE_VDEC_CFG *chnTestParam)
{
    VIM_S32 ret = VIM_SUCCESS;
    memset(&chnTestParam->seqHeader, 0, sizeof(VDEC_SEQHEADER_S));
    VDEC_OUTPUTBUFF_INFO stOutputInfo[16];
    memset(stOutputInfo, 0, sizeof(VDEC_OUTPUTBUFF_INFO)*16);
    stOutputInfo[0].u32SizeHead = 200;
    stOutputInfo[0].u32SizeTail = 511;
    stOutputInfo[1].u32SizeHead = 1888;
    chnTestParam->seqHeader.psOutputBuffInfo = stOutputInfo;
    ret = VIM_MPI_VDEC_StartRecvStream(chnTestParam->VdecChn, &chnTestParam->seqHeader, chnTestParam->blockFlag_StartRecv);
    chnTestParam->seqHeader.psOutputBuffInfo = VIM_NULL;
    return ret;
}

/*  [说明] 终止此通道的解码进程， 此方法执行后， 通道处于刚创建的状态， 此时 feed 方法会失败， 需要重新 StartRecv 使通道活跃起来
    [参数] ...
    [返回值] VIM_SUCCESS 成功； 其它失败    */
VIM_S32 SAMPLE_COMM_VDEC_STOP_RECV(SAMPLE_VDEC_CFG *chnTestParam)
{
    VIM_S32 ret = VIM_FAILURE;
    ret = VIM_MPI_VDEC_StopRecvStream(chnTestParam->VdecChn);
    if (ret != VIM_SUCCESS) {
        VDEC_SAMPLE_ERR("StopChn %d failed",chnTestParam->VdecChn);
    }
    return ret;
}

VIM_S32 SAMPLE_COMM_VDEC_Destroy(SAMPLE_VDEC_CFG *chnTestParam)
{
    VIM_S32 ret = VIM_FAILURE;
    ret = VIM_MPI_VDEC_DestroyChn(chnTestParam->VdecChn);
    if (ret != VIM_SUCCESS) {
        VDEC_SAMPLE_ERR("EndChn %d failed, ret = %x", chnTestParam->VdecChn, ret);
    }
    VDEC_SAMPLE_DEBUG("chn%d destoryed!", chnTestParam->VdecChn);
    chnTestParam->chnStates = SAMPLE_VDEC_CHN_NONE;
    return ret;
}

/*  [说明] 销毁此通道， 此方法执行后， 通道处于未创建的状态， 需要重新 Create 使通道处于可用状态。 除了 Destroy， 此方法还进行了 sample 相关线程的回收工作
    [参数] ...
    [返回值] VIM_SUCCESS 成功； 其它失败    */
VIM_S32 SAMPLE_COMM_VDEC_DESTROY(SAMPLE_VDEC_CFG *chnTestParam)
{
    VIM_S32 ret = VIM_FAILURE;
    VDEC_CHN_STAT_S stStat;
    // 停止接收码流并销毁通道
    if (chnTestParam->chnStates == SAMPLE_VDEC_CHN_NONE) {
        return VIM_FAILURE;
    }
    VIM_MPI_VDEC_Query(chnTestParam->VdecChn, &stStat);
    VDEC_SAMPLE_DEBUG("Stream End! chn = %d, feedCount = %d, decodedFrame = %d, badFrame = %d, left bytes = %d, left frames = %d", chnTestParam->VdecChn, stStat.u32FeedCount, chnTestParam->u32FrameIndex, chnTestParam->u32BadFrameCount, stStat.u32LeftStreamBytes, stStat.u32LeftStreamFrames);
    if (chnTestParam->thread_dec) {
        // pthread_cancel(chnTestParam->thread_dec);
        ret = pthread_join(chnTestParam->thread_dec, VIM_NULL);
        chnTestParam->thread_dec = 0;
    }
    if (chnTestParam->thread_feed) {
        // pthread_cancel(chnTestParam->thread_feed);
        ret = pthread_join(chnTestParam->thread_feed, VIM_NULL);
        chnTestParam->thread_feed = 0;
    }
    ret = SAMPLE_COMM_VDEC_Destroy(chnTestParam);
    ret = VIM_SUCCESS;
    return ret;
}

/*  [说明] 根据配置文件， 创建指定通道 , 并启动接收码流线程、解码线程
    [参数] cfgFile: 配置文件; VdecChn: 指定通道号;
    [返回值] chnNum: 实际创建的通道数， 取决于配置文件中有效配置的数量 和 howmanyChn值, 二者取较小值 */
VIM_S32 SAMPLE_COMM_VDEC_CREATE_ADN_START(VIM_CHAR *cfgFile, VDEC_CHN VdecChn)
{
    VIM_S32 ret = VIM_FAILURE;
    // memset(&testParam[VdecChn], 0, sizeof(SAMPLE_VDEC_CFG));
    testParam[VdecChn].VdecChn = VdecChn;

    ret = SAMPLE_COMM_VDEC_PARSE_CFG(cfgFile, &testParam[VdecChn], VdecChn);
    if (ret) 
        return -1;
    ret = SAMPLE_COMM_VDEC_Create(&testParam[VdecChn]);
    if (ret)
        return -2;
    ret = SAMPLE_COMM_VDEC_START(&testParam[VdecChn], VdecChn);
    if (ret)
        return -3;
    if (testParam[VdecChn].bindFlag == 0)// 如果走绑定流程，则不启动以下get/release线程
    {
        pthread_create(&testParam[VdecChn].thread_dec, NULL, testParam[VdecChn].decode_func? testParam[VdecChn].decode_func: (void*)SAMPLE_COMM_VDEC_TEST_DECODER, &testParam[VdecChn]);
    }
    testParam[VdecChn].chnStates = SAMPLE_VDEC_CHN_RUNNING;
    return ret;
}

/*  [说明] 根据配置文件， 创建所有通道 ， 并启动接收码流线程、解码线程
    [参数] ...; howmanyChn: 期望创建的通道数;
    [返回值] chnNum: 实际创建的通道数， 取决于配置文件中有效配置的数量 和 howmanyChn值, 二者取较小值 */
VIM_S32 SAMPLE_COMM_VDEC_CREATE_ALL_EX(VIM_CHAR *cfgFile, VIM_U8 howmanyChn)
{
    VIM_S32 ret = VIM_SUCCESS, i = 0, chnNum = 0;
    if (howmanyChn == 0 || howmanyChn > MAX_VDEC_CHN) {
        howmanyChn = MAX_VDEC_CHN;
    }
    for (i = 0; i < howmanyChn;  i++) {
        // 从配置文件获取通道i的参数
        ret = SAMPLE_COMM_VDEC_CREATE_ADN_START(cfgFile, i);
        if (ret)
            continue;
        chnNum ++;
    }

    return chnNum;
}

/*  [说明] SAMPLE_COMM_VDEC_CREATE_ALL_EX 的简化版, 启动配置文件中所有通道
    [参数] ...
    [返回值] chnNum: 实际创建的通道数， 取决于配置文件中有效配置的数量 */
VIM_S32 SAMPLE_COMM_VDEC_CREATE_ALL(VIM_CHAR *cfgfile)
{
    return SAMPLE_COMM_VDEC_CREATE_ALL_EX(cfgfile, MAX_VDEC_CHN); 
}

// /*  [说明] 为所有已创建的通道开启 SAMPLE_COMM_VDEC_TEST_DECODER 线程来处理解码输出， 此方法应当在 SAMPLE_COMM_VDEC_CREATE_ALL 之后执行
//     [参数] ...
//     [返回值] chnNum: 实际开始解码的通道的数量 */
// VIM_S32 SAMPLE_COMM_VDEC_START_ALL(){
//     VIM_S32 i = 0, chnNum = 0;
//     for (i = 0; i < MAX_VDEC_CHN; i ++) {
//         if (testParam[i].chnStates == SAMPLE_VDEC_CHN_STARTED) {
//             pthread_create(&testParam[i].thread_dec, NULL, (void*)SAMPLE_COMM_VDEC_TEST_DECODER, &testParam[i]);
//             testParam[i].chnStates = SAMPLE_VDEC_CHN_RUNNING;
//             chnNum ++;
//         }
//     }
//     return chnNum;
// }

/*  [说明] 销毁所有通道
    [参数] ...
    [返回值] VIM_SUCCESS */
VIM_S32 SAMPLE_COMM_VDEC_DESTROY_ALL()
{
    VIM_S32 ret = VIM_SUCCESS, i = 0;
    pthread_t thread_singleshot_void[MAX_VDEC_CHN];
    memset(thread_singleshot_void, 0, sizeof(pthread_t) * MAX_VDEC_CHN);
    flag_end_decode = VIM_TRUE;
    for (i = 0; i < MAX_VDEC_CHN; i ++) {
        if (testParam[i].chnStates != SAMPLE_VDEC_CHN_NONE) {
            pthread_create(&thread_singleshot_void[i], VIM_NULL, (VIM_VOID*)SAMPLE_COMM_VDEC_DESTROY, &testParam[i]);
        }
    }
    for (i = 0; i < MAX_VDEC_CHN; i ++) {
        if (thread_singleshot_void[i])
            pthread_join(thread_singleshot_void[i], VIM_NULL);
    }
    return ret;
}

/*  [说明] 为指定的通道解析配置文件
    [参数] *cfgfilePath 配置文件路径; *returnParam: 解析结果所存放的结构体； chnNo 通道号
    [返回值] ...    */
VIM_S32 SAMPLE_COMM_VDEC_PARSE_CFG(VIM_CHAR *cfgfilePath, SAMPLE_VDEC_CFG *returnParam, VIM_U32 chnNo)
{
    VIM_CHAR pattern_key_val[256];
    memset(pattern_key_val, 0, 256);
    sprintf(pattern_key_val, "%s%d%s", "^[ \\\t]*vdec_chn", chnNo ,"_?(\\w+)[\\\t ]*[:=][\\\t ]*(\\S+)");
    return SAMPLE_COMM_VDEC_PARSE_CFG_EX(cfgfilePath, returnParam, chnNo, pattern_key_val);
}

/*  [说明] 预留方法，为兼容后续的新的配置文件
    [参数] ...
    [返回值] ...    */
VIM_S32 SAMPLE_COMM_VDEC_PARSE_CFG_EX(VIM_CHAR* cfgfilePath, SAMPLE_VDEC_CFG *returnParam, VIM_U32 chnNo, VIM_CHAR* regstring)
{

    VIM_S32 ret = VIM_FAILURE;
    FILE* fp = VIM_NULL;
    VIM_CHAR *cfgContent = VIM_NULL;
    VIM_CHAR result_key[256], result_val[256];
    VIM_CHAR *offset = VIM_NULL, *beginpos = VIM_NULL;
    VIM_U32 tmpVal;
    regex_t re_key_val;
    regmatch_t result[3];
    if (returnParam == VIM_NULL || chnNo >= MAX_VDEC_CHN) {
        VDEC_SAMPLE_ERR("parse cfg file: arg2 == NULL or chnNo >= %d",MAX_VDEC_CHN);
        return -0xf;
    }
    returnParam->VdecChn = chnNo;
    cfgContent = (VIM_CHAR*)malloc(CFG_FILE_MAX_SIZE);
    if (!cfgContent) {
        VDEC_SAMPLE_ERR("parse cfg file: interal malloc failed");
        return -1; 
    }
    beginpos = cfgContent;
    offset = cfgContent;
    memset(cfgContent, 0, CFG_FILE_MAX_SIZE);
    fp = fopen(cfgfilePath, "r");
    if (!fp) {
        VDEC_SAMPLE_ERR("open cfg file: open failed");
        return -2;
    }
    ret = fread(cfgContent, 1, CFG_FILE_MAX_SIZE, fp);
    if (ret <= 0 || ret >= CFG_FILE_MAX_SIZE) {
        VDEC_SAMPLE_ERR("read cfg file: read failed");
        return -3;
    }
    ret = fclose(fp);
    ret = regcomp(&re_key_val, regstring, REG_EXTENDED|REG_NEWLINE|REG_ICASE);
    memset(result, 0, sizeof(regmatch_t)*3);
    while (1){
        memset(result_key, 0, 256);memset(result_val, 0, 256);
        ret = regexec(&re_key_val, offset, 3, result, 0);
        if (ret != 0)
            break;
        memcpy(result_key, offset+result[1].rm_so, result[1].rm_eo - result[1].rm_so);
        memcpy(result_val, offset+result[2].rm_so, result[2].rm_eo - result[2].rm_so);
        VDEC_SAMPLE_DEBUG("chn%d, %s = %s",chnNo , result_key, result_val);
        if (strcasecmp(result_key, "input_File") == 0) {
            memset(returnParam->inputFilepath, 0, 256);
            memcpy(returnParam->inputFilepath, result_val, strlen(result_val));         
        } else if (strcasecmp(result_key, "input_DecodeType") == 0) {
            switch (result_val[0]) {
                case 's':
                    returnParam->chnAttr.enCodType = PT_SVAC2;
                    break;
                case 'h':
                    returnParam->chnAttr.enCodType = PT_H265;
                    break;
                default:
                    returnParam->chnAttr.enCodType = PT_SVAC2;
            }
        } else if (strcasecmp(result_key, "input_ExtraFrame") == 0) {
            returnParam->chnAttr.u32ExtraFrameBufferNum = atoi(result_val);
        } else if (strcasecmp(result_key, "input_StreamSize") == 0) {
            returnParam->chnAttr.u32StreamSize = atoi(result_val);
        } else if (strcasecmp(result_key, "output_UsePicWidth") == 0) {
            returnParam->savePicWidth = 1;
        } else if (strcasecmp(result_key, "output_loop_count") == 0) {
            returnParam->feedLoopCount = atoi(result_val); 
        } else if (strcasecmp(result_key, "output_File_PicWidth") == 0) {
            tmpVal = atoi(result_val);
            if (tmpVal > 0)
                returnParam->saveResize.u32Width = tmpVal;
        } else if (strcasecmp(result_key, "output_File_PicHeight") == 0) {
            tmpVal = atoi(result_val);
            if (tmpVal > 0)
                returnParam->saveResize.u32Height = tmpVal;
        } else if (strcasecmp(result_key, "output_File") == 0) {
            memset(returnParam->outputFilepath, 0, 256);
            memcpy(returnParam->outputFilepath, result_val, strlen(result_val));
        } else if (strcasecmp(result_key, "output_Bind") == 0) {
            returnParam->bindFlag = 1;
        } else if (strcasecmp(result_key, "output_PixelFormat") == 0) {
            returnParam->chnAttr.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            if (strcasecmp(result_val, "yuv420p10bit16bit") == 0) {
                returnParam->chnAttr.enPixelFormat = PIXEL_FORMAT_YUV_PLANAR_420_P10_2BYTE_LSB;
            } else if (strcasecmp(result_val, "yuv420p10bit") == 0) {
                returnParam->chnAttr.enPixelFormat = PIXEL_FORMAT_YUV_PLANAR_420_P10;
            } else if (strcasecmp(result_val, "yuv420sp10bit16bit") == 0) {
                returnParam->chnAttr.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10_2BYTE_LSB;
            } else if (strcasecmp(result_val, "yuv420sp10bit") == 0) {
                returnParam->chnAttr.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420_P10;
            } else if (strcasecmp(result_val, "yuv420p") == 0) {
                returnParam->chnAttr.enPixelFormat = PIXEL_FORMAT_YUV_PLANAR_420;
            } else if (strcasecmp(result_val, "yuv420sp") == 0) {
                returnParam->chnAttr.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            }
        } else if (strcasecmp(result_key, "input_UserdataSize") == 0) {
            returnParam->chnAttr.u32UserDataSize = atoi(result_val);
        } else if (strcasecmp(result_key, "input_Thumbnail") == 0) {
            if (atoi(result_val))
                returnParam->chnAttr.u32CtrlMode |= CTRL_MODE_FULL;
        } else if (strcasecmp(result_key, "input_FrameRate") == 0) {
            returnParam->chnAttr.u32FrameRate = atoi(result_val);
        } else if (strcasecmp(result_key, "input_PicWidth") == 0) {
            returnParam->chnAttr.u32PicWidth = atoi(result_val);
        } else if (strcasecmp(result_key, "input_PicHeight") == 0) {
            returnParam->chnAttr.u32PicHeight = atoi(result_val);
        } else if (strcasecmp(result_key, "input_Priority") == 0) {
            returnParam->chnAttr.u32Priority = atoi(result_val);
        } else if (strcasecmp(result_key, "input_FeedingMode") == 0) {
            switch (result_val[1]) {
                case 'r':
                case 'R':
                    returnParam->chnAttr.enFeedingMode = FEEDING_MODE_FRAME_SIZE;
                    returnParam->chnAttr.u8Enable_FrameInfo = 1;
                    break;
                case 'i':
                case 'I':
                    returnParam->chnAttr.enFeedingMode = FEEDING_MODE_FIXED_SIZE;
                    returnParam->chnAttr.u8Enable_FrameInfo = 0;
                    break;
            }
        } else if (strcasecmp(result_key, "input_SkipMode") == 0) {
            switch (result_val[3]) {
                case 'm': case 'M':
                    returnParam->chnAttr.enSkipMode = 0;
                    break;
                default:
                case 'p': case 'P':
                    returnParam->chnAttr.enSkipMode = 0x1;
                    break;
            }
        }
        offset += result[0].rm_eo;
    }
    regfree(&re_key_val);
    free(cfgContent);
    if (offset != beginpos)
        return VIM_SUCCESS;
    else{
        // VDEC_SAMPLE_ERR("parse cfg: no cfg found for chn%d",chnNo);
        return -6;
    }
}
