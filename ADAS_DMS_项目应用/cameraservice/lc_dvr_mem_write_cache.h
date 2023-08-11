#ifndef _LC_DVR_MEM_WRITE_CACHE_H_
#define _LC_DVR_MEM_WRITE_CACHE_H_
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#include <stdlib.h> /**loff_t的头文件**/
#include <stdint.h>
#include "lc_dvr_glob_def.h"

void lc_dvr_mem_write_cache_initial(void);
void lc_dvr_mem_write_cache_release(void);
int lc_dvr_mem_cam_busy_status(void);
void lc_dvr_mem_get_callback_para(int *camId, int *Index);
void* lc_dvr_mem_write_cache_open(char* filePath, char *modes);
int  lc_dvr_mem_write_cache_write(void* stream, void* input, int len);
int  lc_dvr_mem_write_cache_seek(void* stream, uint32_t offset);
int  lc_dvr_mem_write_cache_tell(void* stream);
int  lc_dvr_mem_write_cache_close(void* stream, int *size);
int  lc_dvr_mem_video_file_read(int camId, int index, char* dir, char *fileNmae);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif



