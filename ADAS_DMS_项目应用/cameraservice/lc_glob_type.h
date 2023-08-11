#ifndef _LC_GLOB_TYPE_H_
#define _LC_GLOB_TYPE_H_
#ifdef __cplusplus
extern "C" {
#endif
//#define LC_MDVR_FUNCTION_TEST /**���ܲ���ʹ��**/

#define LC_STREAM_ENCODE_CHANEL 0
#define LC_LARGE_ENCODE_CHANEL  1
#define LC_SMALL_ENCODE_CHANEL  2
#define LC_THUMB_ENCODE_CHANEL  3
#define LC_MDVR_RECORD_ENABLE 
#define OSD_ENABLE
#define PACK_H265 	/*�򿪴˺�ʹ��H265����, ����ʹ��SVAC����*/

#ifdef PACK_H265
#define EXT_VIDEO_MP4 			".MP4"
#else
#define EXT_VIDEO_MP4 			".SVC"
#endif

#define LC_DVR_DURATION 60*1
/**���Թ��������д�����ȶ�**/
#define LC_DVR_BITRATE  1536
/**��Ƶ¼�Ƶķֱ���**/
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
	char  fileType;		/**��Ƶ���� 0. ����¼�� 1. ��ͨ�¼���¼ 2. �����¼���¼**/
	char  camChnl;		/**����ͷ��ͨ��**/
	short offset;  		/**��ȡʱ��Ϊ��ʼ��ţ�����ʱ��ֵΪ���һ���ļ������**/
	short bytePerFile; 	/**����ʱ��Ч����ʾfileListÿ���ļ�������ֽڳ���**/
	short fileNumber;	/**��ȡʱΪ�������ص��ļ�����������ʱΪʵ�ʵ��ļ�����**/
	char  fileDir[56];	/**�ļ�����Ŀ¼��·��**/
	char  fileList[64]; /**¼���ļ������ƣ�ͬĿ¼������Ŀ¼.thumb�����е�ͼƬ��¼���ļ���һ��**/
}LC_RECORD_LIST_FETCH;

typedef struct{
	int  pic_width;			/**��Ҫ��ͼƬ�ֱ���**/
	int  pic_height;		
	int  camChnl;			/**����ͷͨ��**/
	int  sizeMode;
	int  frameDelay;		/**��ʱ���㣬Ԥ��**/
	char pic_save_path[128];/**��ȡ����ͷͼ���Ĵ洢·��**/
	void* ackFun;
}LC_TAKE_PIC_INFO;

typedef struct{
	int resolution;			/**Ԥ����Ƶ���ķֱ���**/
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

