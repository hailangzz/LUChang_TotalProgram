#ifndef __SAMPLE_COMM_VDEC_H__
#define __SAMPLE_COMM_VDEC_H__
#define __SAMPLE_COMM_VDEC_UTF_8__

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <signal.h>
#include <regex.h>
#include <poll.h>
#include <fcntl.h>

#include "mpi_vdec.h"

#define FEEDING_SIZE                0x40000
#define VDEC_SAMPLE_DEBUG(...)      printf("\r\n[VDEC_SAMPLE_DEBUG] ");printf(__VA_ARGS__)
#define VDEC_SAMPLE_WARN(...)       printf("\r\n\x1b[33m[VDEC_SAMPLE_WARN]\x1b[0m ");printf(__VA_ARGS__)
#define VDEC_SAMPLE_ERR(...)        printf("\r\n\x1b[41m[VDEC_SAMPLE_ERR]\x1b[0m ");printf(__VA_ARGS__)
#define MAX_VDEC_CHN                8
#define MAX_PATH_LENGTH             256
#define CFG_FILE_MAX_SIZE           1024*1024
#define SAMPLE_VDEC_FPS_CALC_UNIT   5000

/*
    [说明] 此状态仅为sample控制流程使用，与通道的实际状态无关
*/

typedef enum {
    SAMPLE_VDEC_NALU_TYPE_SVAC_RESERVED0    = 0,
    SAMPLE_VDEC_NALU_TYPE_SVAC_NON_ISLICE   = 1,
    SAMPLE_VDEC_NALU_TYPE_SVAC_ISLICE       = 2,
    SAMPLE_VDEC_NALU_TYPE_SVAC_NON_ISLICE_EL    = 3,
    SAMPLE_VDEC_NALU_TYPE_SVAC_ISLICE_EL    = 4,
    SAMPLE_VDEC_NALU_TYPE_SVAC_SURVEILLANCE_EX  = 5,
    SAMPLE_VDEC_NALU_TYPE_SVAC_SEU  = 6,
    SAMPLE_VDEC_NALU_TYPE_SVAC_SEI  = 6,
    SAMPLE_VDEC_NALU_TYPE_SVAC_SPS  = 7,
    SAMPLE_VDEC_NALU_TYPE_SVAC_PPS  = 8,
    SAMPLE_VDEC_NALU_TYPE_SVAC_SEC  = 9,
    SAMPLE_VDEC_NALU_TYPE_SVAC_AUTH_DATA    = 10,
    SAMPLE_VDEC_NALU_TYPE_SVAC_STREAM_END   = 11,
    SAMPLE_VDEC_NALU_TYPE_SVAC_AUDIO    = 13,
    SAMPLE_VDEC_NALU_TYPE_SVAC_PPS_EL   = 15,
//  ---------------------------------------------------
    SAMPLE_VDEC_NALU_TYPE_HEVC_TRAIL_N  = 0,
    SAMPLE_VDEC_NALU_TYPE_HEVC_TRAIL_R  = 1,
    SAMPLE_VDEC_NALU_TYPE_HEVC_TSA_N    = 2,
    SAMPLE_VDEC_NALU_TYPE_HEVC_TSA_R    = 3,
    SAMPLE_VDEC_NALU_TYPE_HEVC_STSA_N   = 4,
    SAMPLE_VDEC_NALU_TYPE_HEVC_STSA_R   = 5,
    SAMPLE_VDEC_NALU_TYPE_HEVC_RADL_N   = 6,
    SAMPLE_VDEC_NALU_TYPE_HEVC_RADL_R   = 7,
    SAMPLE_VDEC_NALU_TYPE_HEVC_BLA_W_LP     = 16,
    SAMPLE_VDEC_NALU_TYPE_HEVC_BLA_W_RADL   = 17,
    SAMPLE_VDEC_NALU_TYPE_HEVC_BLA_N_LP = 18,
    SAMPLE_VDEC_NALU_TYPE_HEVC_IDR_W_RADL   = 19,
    SAMPLE_VDEC_NALU_TYPE_HEVC_IDR_N_LP = 20,
    SAMPLE_VDEC_NALU_TYPE_HEVC_CRA_NUT  = 21,
    SAMPLE_VDEC_NALU_TYPE_HEVC_VPS_NUT  = 32,
    SAMPLE_VDEC_NALU_TYPE_HEVC_SPS_NUT  = 33,
    SAMPLE_VDEC_NALU_TYPE_HEVC_PPS_NUT  = 34,
    SAMPLE_VDEC_NALU_TYPE_HEVC_AUD_NUT  = 35,
    SAMPLE_VDEC_NALU_TYPE_HEVC_EOS_NUT  = 36,
    SAMPLE_VDEC_NALU_TYPE_HEVC_EOB_NUT  = 37,
    SAMPLE_VDEC_NALU_TYPE_HEVC_FD_NUT   = 38,
    SAMPLE_VDEC_NALU_TYPE_HEVC_PREFIX_SEI_NUT = 39,
    SAMPLE_VDEC_NALU_TYPE_HEVC_SUFFIX_SEI_NUT = 40,
    SAMPLE_VDEC_NALU_TYPE_MAX
} SAMPLE_VDEC_NALU_TYPE;

typedef enum{
    SAMPLE_VDEC_CHN_NONE = 0,
    SAMPLE_VDEC_CHN_CREATED,
    SAMPLE_VDEC_CHN_STARTED,
    SAMPLE_VDEC_CHN_RUNNING, 
    SAMPLE_VDEC_CHN_PAUSE,
    SAMPLE_VDEC_CHN_END,
} SAMPLE_VDEC_CHN_STATES;

typedef struct {
    VIM_U32 payload_size;
    VIM_U32 total_size;
    SAMPLE_VDEC_NALU_TYPE nalu_type;
} SAMPLE_VDEC_NALU_INFO;

typedef struct{
    VDEC_CHN VdecChn;
    SAMPLE_VDEC_CHN_STATES chnStates;
    VIM_U8 savePicWidth;
    RECT_S saveResize;
    VDEC_CHN_ATTR_S chnAttr;
    VDEC_SEQHEADER_S seqHeader;
    FILE* codingFile;
    // VIM_VOID (*feed_func)(SAMPLE_VDEC_CFG*);
    VIM_S32 (*usb_feedfunc)(VIM_VOID*);
    VIM_S32 (*feed_func)(VIM_VOID*);
    VIM_S32 (*decode_func)(VIM_VOID*);
    VIM_S32 (*output_func)(VDEC_CHN, VDEC_PACK_S*);
    VIM_CHAR inputFilepath[MAX_PATH_LENGTH];
    VIM_CHAR outputFilepath[MAX_PATH_LENGTH];
    VIM_S32 feedLoopCount;
    VIM_S32 captureFlag;
    VIM_S32 skipFlag;
    VIM_S32 bindFlag;
    VIM_S32 blockFlag_StartRecv;
    VIM_S32 blockFlag_GetFrame;
    VIM_U32 u32FeedIndex;
    VIM_U32 u32FrameIndex;
    VIM_U32 u32BadFrameCount;
    pthread_t thread_feed;
    pthread_t thread_dec;
} SAMPLE_VDEC_CFG;

VIM_S32 SAMPLE_COMM_VDEC_Init();
VIM_S32 SAMPLE_COMM_VDEC_Exit();
VIM_S32 SAMPLE_COMM_VDEC_Create(SAMPLE_VDEC_CFG *chnTestParam);
VIM_S32 SAMPLE_COMM_VDEC_START(SAMPLE_VDEC_CFG *chnTestParam, VDEC_CHN VdecChn);
VIM_S32 SAMPLE_COMM_VDEC_START_FEEDING(SAMPLE_VDEC_CFG *chnTestParam);
VIM_S32 SAMPLE_COMM_VDEC_START_RECV(SAMPLE_VDEC_CFG *chnTestParam);
VIM_S32 SAMPLE_COMM_VDEC_STOP_RECV(SAMPLE_VDEC_CFG *chnTestParam);
VIM_S32 SAMPLE_COMM_VDEC_Destroy(SAMPLE_VDEC_CFG *chnTestParam);
VIM_S32 SAMPLE_COMM_VDEC_DESTROY(SAMPLE_VDEC_CFG *chnTestParam);
VIM_S32 SAMPLE_COMM_VDEC_PARSE_CFG(VIM_CHAR* cfgfilePath, SAMPLE_VDEC_CFG *returnParam, VIM_U32 chnNo);
VIM_S32 SAMPLE_COMM_VDEC_PARSE_CFG_EX(VIM_CHAR* cfgfilePath, SAMPLE_VDEC_CFG *returnParam, VIM_U32 chnNo, VIM_CHAR* regstring);
VIM_S32 SAMPLE_COMM_VDEC_CREATE_ALL(VIM_CHAR *cfgFile);
VIM_S32 SAMPLE_COMM_VDEC_CREATE_ALL_EX(VIM_CHAR *cfgFile, VIM_U8 howmanyChn);
VIM_S32 SAMPLE_COMM_VDEC_CREATE_ADN_START(VIM_CHAR *cfgFile, VDEC_CHN VdecChn);
VIM_S32 SAMPLE_COMM_VDEC_DESTROY_ALL();
VIM_S32 SAMPLE_COMM_VDEC_TEST_DECODER(SAMPLE_VDEC_CFG *chnTestParam);
VIM_U32 SAMPLE_COMM_VDEC_FEEDING_STREAM(SAMPLE_VDEC_CFG* chnTestParam);
VIM_S32 SAMPLE_COMM_VDEC_FeedStream(SAMPLE_VDEC_CFG *chnTestParam, VDEC_STREAM_S *sVdecStream, VIM_U8 flag_retry);
VIM_U32 SAMPLE_COMM_VDEC_GetOneFrameFromFile(FILE *codingFile, VIM_U8 *pu8Addr, PAYLOAD_TYPE_E codeType);
VIM_S32 SAMPLE_COMM_VDEC_GetEndFlag();
VIM_VOID SAMPLE_COMM_VDEC_SetEndFlag(VIM_S32 endFlag);
SAMPLE_VDEC_CFG* SAMPLE_COMM_VDEC_GetTestParam(VIM_S32 index);

#ifdef __cplusplus
}
#endif

#endif