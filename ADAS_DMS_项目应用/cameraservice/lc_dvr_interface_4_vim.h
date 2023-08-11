#ifndef _LC_DVR_INTERFACE_4_VIM_H_
#define _LC_DVR_INTERFACE_4_VIM_H_
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */
#include "lc_glob_type.h"

void vc_loop_image_ctrl(void* imgInfo);
void vc_loop_image_handle(void);
VIM_VOID vc_record_para_initial(void);
VIM_VOID vc_record_para_set(void* config);
VIM_VOID *vc_record_para_get(void);
VIM_VOID vc_set_mdvr_record_status(int stauts);
VIM_VOID vc_start_mediaFile_play(char* fileName);
VIM_VOID vc_start_mediaFile_stop(void);
VIM_VOID vc_media_play_seek(double time);
VIM_VOID vc_media_play_pause(int status);
VIM_VOID vc_media_play_speed(int speed);
VIM_VOID vc_media_play_status_notify(int status);
VIM_S32 vc_jpeg_enc_initial(int width, int height, int chanel, int level);

VIM_VOID vc_set_record_status(int val);
VIM_S32 vc_get_record_status(void);
VIM_VOID vc_set_AHDDetect_status(int val);
VIM_S32 vc_get_AHDDetect_status(void);
VIM_VOID vc_chn_record_start(void);
VIM_S32 vc_chn_record_stop(void);

VIM_VOID *vc_vim_file_handle(void);
VIM_VOID vc_stream_out_start(int camChnl, int on_off, int index);
VIM_VOID vc_stream_out_stop(int index);
VIM_VOID vc_stream_out_chnlSwitch(int camChnl, int index);
VIM_S32 vc_stream_venc_initial(void *venc_cfg, int index);
VIM_S32 vc_stream_venc_unInitial(int index);
VIM_S32 vc_stream_venc_config(int resolution, int index);
VIM_S32 vc_stream_venc_destroy(int index);
VIM_S32 vc_stream_venc_chnl(int index);
void *vc_stream_handler(void *arg);
int  vc_vim_copy_video_file_to_dist(int chanel, int index, char* dstDir, char* name);
LC_VENC_STREAM_INFO* vc_stream_venc_resolution_get(int index);
char* vc_dynamic_mount_path(void);
void* vc_video_file_copy_thread(void* arg);
void  vc_video_file_copy_break(void);
int   vc_video_file_copy_status(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif

