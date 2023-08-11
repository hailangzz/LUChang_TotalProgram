#ifndef _DUVONN_STORAGE_H_
#define _DUVONN_STORAGE_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "lc_dvr_mmc.h"
#define LC_DUVONN_CAMERA_NUMBER 	2

typedef struct {
	uint32_t index;				/**文件序号**/
	uint32_t sector_start;		/**文件内容所在扇区的位置**/
	uint32_t fileSize;			/**文件的总长度**/
	uint32_t fileNameSector;	/**文件名所在扇区的标号**/
	uint32_t fileNameOffset;	/**文件名在扇区中的偏移量**/	
	uint32_t curPosition;
	uint32_t sector_current;
	uint32_t dirty;				/**当前的cache有被修改过**/
}DUVONN_FILE_LIST;

typedef struct {
	DUVONN_FILE_LIST fileInfo;
	char fileName[128];
}DUVONN_RECORD_LIST;

DUVONN_RECORD_LIST* duvonn_camera_get_curfile_info(int camID);
uint16_t duvonn_exFAT_NameHashCal(uint16_t hashIn, void *fileName, int nameLength);
void duvonn_suffering_initial(int cameraNumber, char *license);
void duvonn_suffering_unInitial(void);
int  duvonn_camera_creat_new_file(int camID);
int  duvonn_ctrl_write(void* wrBuf, int len, void* handle);
int  duvonn_ctrl_read(void* rdBuf, int len, void* handle);
int  duvonn_ctrl_seek(void* handle, int offset, int from);
void duvonn_ctrl_fflush(void* handle);
void *duvonn_ctrl_open(int rcType, char para[]);
void duvonn_ctrl_close(void* handle);
int  duvonn_get_video_tail(int camID, int index);
void *duvonn_get_video_name(void* handle);
int  duvonn_video_get_list_info(int camID, int* curIdx);
int  duvonn_video_write(int camID, int index, void* inBuf, int len, int offset);
int  duvonn_video_read(int camID, int index, void* inBuf, int len, int offset);
char* duvonn_video_name_get(int camID, int index);
void duvonn_storage_test(void);
uint8_t* URL_2_GBK(void *inUrl);
int duvonn_camera_reset_file(int camID);

#ifdef __cplusplus
}
#endif
#endif

