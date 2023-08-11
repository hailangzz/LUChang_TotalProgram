#ifndef __RECORDCAMERA_H__
#define __RECORDCAMERA_H__
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */
/************class******************/
#define PIC_NAME_LEN 128
#define MAX_PIC_NUM 512
#define CM_MAX_PICTURE_NUM              1024
#define CM_SAVE_FILE_MARK "save"
#define CM_LOCK_FILE_MARK "lock"
#define CM_MAX_RECORDFILE_NUM (480)
#define CM_MAX_EMERGENCY_NUM (48*4)
#define SOUND_REC_FILE_NUM	160
#define IMAGE_REC_FILE_NUM	1024

// (8*60)
#define CM_MAX_FILE_LEN 80
#define CAMERA_360_ID 9
#define CM_THUMB_DIR ".thumb/"
#define LOCK_VIDEO    "lockVideo"
#define PARK_MONITOR  "parkMonitor"
#define EXT_PIC_JPG ".jpg"
#define EXT_PIC_BMP ".bmp"
#define EXT_PIC EXT_PIC_JPG
typedef struct{ //
	uint8_t		type	:2;
	uint32_t	size	:30;
}recordInfo_size_type;	/*表A.8 开关量信息位定义*/

typedef struct recordInfo
{
	uint32_t  size;	/**高两位表示视频记录的类型，低30位表示视频记录的长度**/
	char startTime[6];
	char stopTime[6];
	char information[32];
}recordInfo_t;

typedef struct recordInfoExt_t
{
	recordInfo_t info;
	char cur_filename[CM_MAX_FILE_LEN];
}recordInfoExt;

typedef struct file_status
{
    int cur_file_idx;
    int cur_max_filenum;
    int timeout_second;
    // char cur_filename[CM_MAX_RECORDFILE_NUM][CM_MAX_FILE_LEN];
    char cur_filedir[CM_MAX_FILE_LEN];
	// recordInfo_t info[CM_MAX_RECORDFILE_NUM];
	char vehicle_license[32];
    int cur_dir_file_num;
	recordInfoExt fileInfo[16];
} file_status_t;

typedef struct
{
    unsigned int  encode_frame_num;
    unsigned int  encode_format;
    unsigned int  bit_rate;
    int frame_rate;
    int maxKeyFrame;
    long long pts;
}encode_param_t;

typedef struct 
{   
    int mCameraId;    
    int mDuration;
    int storage_state;
    int iMaxPicNum;
    char **ppSuffix;
    encode_param_t	encode_param;
    file_status_t FileManger;
}RecordCamera;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif
