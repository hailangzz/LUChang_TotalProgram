#ifndef _LC_GLOB_TYPE_H_
#define _LC_GLOB_TYPE_H_
#ifdef __cplusplus
extern "C" {
#endif
//#define LC_MDVR_FUNCTION_TEST /**功能测试使能**/

#define LC_STREAM_ENCODE_CHANEL 0
#define LC_LARGE_ENCODE_CHANEL  1
#define LC_SMALL_ENCODE_CHANEL  2
#define LC_THUMB_ENCODE_CHANEL  3
#define LC_MDVR_RECORD_ENABLE 
#define OSD_ENABLE
#define PACK_H265 	/*打开此宏使用H265编码, 否则使用SVAC编码*/

#ifdef PACK_H265
#define EXT_VIDEO_MP4 			".MP4"
#else
#define EXT_VIDEO_MP4 			".SVC"
#endif

#define LC_DVR_DURATION 60*1
/**测试过这个码率写卡最稳定**/
#define LC_DVR_BITRATE  1536
/**视频录制的分辨率**/
#define LC_RECORD_VIDEO_NSIZE_W	1280
#define LC_RECORD_VIDEO_NSIZE_H	720
#define LC_RECORD_VIDEO_HSIZE_W	1920
#define LC_RECORD_VIDEO_HSIZE_H	1080

//#define LC_DVR_RECORD_BASE_DIR "/mnt/nfs/record"
#define LC_DVR_RECORD_BASE_DIR "/mnt/intsd"
//#define LC_DVR_RECORD_BASE_DIR "/mnt/usb/usb1"

#define LC_TAKE_FULL_PIC_SIZE_W 2560
#define LC_TAKE_FULL_PIC_SIZE_H 1440

#define LC_TAKE_LARGE_PIC_SIZE_W 1920
#define LC_TAKE_LARGE_PIC_SIZE_H 1088

#define LC_TAKE_STREAM_PIC_SIZE_W 960
#define LC_TAKE_STREAM_PIC_SIZE_H 544

#define LC_TAKE_SMALL_PIC_SIZE_W 1280
#define LC_TAKE_SMALL_PIC_SIZE_H 736

#define LC_TAKE_THUMB_PIC_SIZE_W 640
#define LC_TAKE_THUMB_PIC_SIZE_H 384

#define LC_DVR_JPEG_GROUP_ID 	2
#define LC_DVR_JPEG_CHANEL_ID 	5

#define LC_DUVONN_CHANEL_0 		0
#define LC_DUVONN_CHANEL_1 		1

#define CMA_DMS 	'D'
#define CMA_ADAS 	'F'
#define CMA_BRAKE 	'P'
#define CMA_REAR 	'R'

enum{
	STREAM_DUMMY_ENU,
	STREAM_1280X720_ENU,
	STREAM_1920X1080_ENU,
	STREAM_960X544_ENU,
	STREAM_MAX_ENU,
};//VENC_STREAM_RES

enum{
	PIC_MODE_THUMB,
	PIC_MODE_WONDERFUL,
	PIC_MODE_MAX,
};//PIC_SIZE_MODE_ENU

typedef struct{
	char  fileType;		/**视频类型 0. 正常录像 1. 普通事件记录 2. 紧急事件记录**/
	char  camChnl;		/**摄像头的通道**/
	short offset;  		/**获取时作为起始序号，返回时其值为最后一个文件的序号**/
	short bytePerFile; 	/**返回时有效，表示fileList每个文件的最大字节长度**/
	short fileNumber;	/**获取时为期望返回的文件数量，返回时为实际的文件数量**/
	char  fileDir[56];	/**文件所在目录的路径**/
	char  fileList[64]; /**录像文件的名称，同目录下有子目录.thumb，其中的图片跟录像文件名一致**/
}LC_RECORD_LIST_FETCH;

typedef struct{
	int  pic_width;			/**需要的图片分辨率**/
	int  pic_height;		
	int  camChnl;			/**摄像头通道**/
	int  sizeMode;
	int  frameDelay;		/**延时拍摄，预留**/
	char pic_save_path[128];/**获取摄像头图像后的存储路径**/
	void* ackFun;
}LC_TAKE_PIC_INFO;

typedef struct{
	int resolution;			/**预览视频流的分辨率**/
	int camChnl;
	int on_off;
	int venc_used;
	int codeChnl;
}LC_VENC_STREAM_INFO;

typedef struct{
	int  size;
	char startTime[6];
	char stopTime[6];
	int  handle;
	int  index;
}FileInfomation;

typedef struct
{
	int duration;
	int birate;
	int mute;
	int photosize;
}LC_DVR_REC_DB_SAVE;

#ifdef __cplusplus
}
#endif
#endif

