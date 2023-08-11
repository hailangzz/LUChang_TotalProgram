#ifndef _LC_CODEC_API_H_
#define _LC_CODEC_API_H_
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */
#include "sample_venc_cfgParser.h"
#include "mpi_vpss.h"
#include "mpi_vb.h"

VIM_VOID lc_venc_para_from_file(SAMPLE_ENC_CFG *encCfg, VIM_S32 chanl);
VIM_S32 lc_venc_sendFrame_getStream(VIM_U32 VencChn, VIM_VPSS_FRAME_INFO_S *stFrameInfo);
VIM_S32 lc_venc_open(SAMPLE_ENC_CFG *venc_cfg_config);
VIM_S32 lc_venc_close(VENC_CHN VencChn);
VIM_S32 lc_venc_pipe_creat(int index);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif
