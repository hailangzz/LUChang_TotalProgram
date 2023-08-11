#ifndef _VIMMEDIATYPE_H_
#define _VIMMEDIATYPE_H_
#include<stdio.h>
#include <stdint.h>

typedef void* MPSHANDLE;

typedef enum
{
	AV_TYPE_NONE = 0,
	AV_TYPE_VIDEO,	//<video
	AV_TYPE_AUDIO	//<audio
}AV_TYPE;

typedef enum
{
	AV_FMT_NONE = 0,
	AV_FMT_H264,	//<H264
	AV_FMT_H265,	//<h265
	AV_FMT_SVAC2,	//<svac2
	AV_FMT_AAC,		//<AAC
	AV_FMT_END
}AV_CODEC_FMT;

typedef enum
{
	PIXEL_FMT_I420 = 0,	//=>YUV420P:YYYYYYYY UU VV
	PIXEL_FMT_NV12,		//=>YUV420SP:YYYYYYYY UV UV
	PIXEL_FMT_YV12,		//=>YUV420P:YYYYYYYY VV UU
	PIXEL_FMT_NV21,		//=>YUV420SP:YYYYYYYY VU VU

	SAMPLE_BIT_U8 = 10,
	SAMPLE_BIT_S16_LE,
	SAMPLE_BIT_S32_LE,
	SAMPLE_BIT_FLT_LE
}AV_FRAME_FMT;

typedef struct
{
	AV_FRAME_FMT sampleFmt;	//音频采样位数
	int sampleRate;			//音频采样率
	int channel;			//音频通道数
}AFrameInfo;

typedef struct
{
	AV_FRAME_FMT pixelFmt;
	int width;				//视频帧宽
	int height;				//视频帧高
	int frameRate;			//帧率
}VFrameInfo;

typedef enum
{
	AV_CB_EVENT_DATA = 0, 	//AV数据包
	AV_CB_EVENT_SEEK, 		//AV数据包seek,仅仅demuxer中用到此事件
	AV_CB_EVNET_ERROR,		//媒体源出错
	AV_CB_EVENT_END      	//媒体源结束
}AV_CB_EVENT;

//编码通道0~7
#define MIN_VENC_CH 0
#define MAX_VENC_CH 7
//录像通道0~15
#define MIN_REC_CH 0
#define MAX_REC_CH 15
//录像时长10s~3600s
#define MIN_REC_TIMES 10
#define MAX_REC_TIMES 3600

typedef int (*VencPacketCb)(int chId, AV_CODEC_FMT fmt, unsigned char* data, int len, long long pts, long long dts, int flag, void* userData);
typedef int (*JpegPacketCb)(int chId, unsigned char* data, int len, int w, int h, long long pts, void* userData);

//音频输出回调，接口里不能长时间阻塞
typedef int (*AudioFrameCb)(const AFrameInfo* info, const unsigned char* data, int len, long long pts, void* userData);

//录像视频源类型
typedef enum 
{
	PUSH_FRAME_TYPE = 0, 	//用户手动推送yuvframe数据
	BIND_VPSS_TYPE,			//录像通道自动绑定VPSS
}RECORD_SOURCE;

//vim模块的组ID和通道ID
typedef struct
{
	int devId;
	int chId;
}VimModule;

typedef struct
{
	double duration;	//录像时长，单位为秒，作为启动录像参数时范围为10~3600
	int bitRate;		//码率，单位为kbps
	AV_CODEC_FMT vFmt;	//视频编码格式，目前仅支持：AV_FMT_H265
	AV_CODEC_FMT aFmt;	//音频编码格式，目前仅支持：AV_FMT_AAC
    VFrameInfo vInfo;	//视频通道信息
	AFrameInfo aInfo;	//音频通道信息
	char vEncConfig;	//加密参数(目前只支持视频svac2加密和音频aac加密)，vEncConfig的bit位第一位表示视频是否加密，第五为表示音频是否加密
						//目前支持的值：音视频都不加密：0x0,音视频都加密：0x11,视频加密音频不加密：0x01
}MediaInfo;

typedef struct
{
	AV_TYPE type;
	AV_CODEC_FMT fmt;
	long long pts;
	long long dts;
	unsigned char *data;
	unsigned int len;
	char flags; //关键帧标志
}MpsPacket;

/**
 * @brief 动态获取录像全路径，包括文件名(以.mp4结尾)，目录不行存在
 * @param ch 输入参数，对应录像通道ID
 * @param event 输入参数，0位定时录像，大于0对应事件录像的event值
 * @param fullpath 输出参数，返回录像全路径，长度不能超过1024，否则为未定义操作
 * @note 用户需要保证此接口不被阻塞，并且返回的录像路径不重复
 * @return 成功：0，错误码：暂无
 */
typedef int (*GetRecFullPathCb)(int ch, int event, char* fullpath);

//player视频输出模块通道信息
typedef struct
{
	int type;			//模块类型，目前仅支持0：vpss模块、1：vo模块
	int devId;			//模块设备号
    int chId;			//模块通道号
    AV_FRAME_FMT vFmt;	//模块的输入视频格式：PIXEL_FMT_I420:yuv420p和PIXEL_FMT_NV12:yuv420sp
}VoModInfo;

/**
 * @brief 播放状态回调接口
 * @param handle 播放句柄
 * @param event 播放状态事件
 * @param data 状态数据，当event值为PLAY_PROG_EVNET时，次参数有效，为double类型指针，为当前播放时间点
 * @param userData 用户数据
 * @note
 * @return 成功：0，错误码：暂无
 */
typedef int(*PlayCallBack)(int handle, int event, void*data, void*userData);

//录像播放状态事件
typedef enum 
{
	PLAY_START_EVNET = 0,	//播放开始
	PLAY_PROG_EVNET,		//播放进度
	PLAY_OVER_EVENT,		//播放结束
	PLAY_ERROR_EVENT,		//播放出错
};

//播放控制命令
typedef enum 
{
	PLAY_PAUSE_CMD = 0,	//暂停播放
	PLAY_START_CMD,		//暂停后继续播放
	PLAY_SEEK_CMD,		//播放跳转
	PLAY_SPEED_CMD,		//播放速度控制，只支持倍速播放
};

// rtsp事件回调结构体
typedef struct RtspEventHandler
{
	int (*OnStart)();//返回0表示接受rtsp请求，否则拒绝请求,可以用来控制同时连接的rtsp客户端个数
	void (*OnStop)();
} RtspEventHandler;

// 外部读写接口结构体
typedef struct ExtRwHanlder
{
	//接口参数和返回值需要兼容C文件读写接口，所有接口都要实现
	void *(*open)(const char *fileName, const char *modes);
	size_t (*read)(void *param, size_t size, size_t n, void *stream);
	size_t (*write)(const void *ptr, size_t size, size_t n, void *stream);
	int (*seek)(void *stream, __off64_t off, int whence);
	__off64_t (*tell)(void *stream);
	int (*close)(void *stream);
} ExtRwHanlder;

//viss平台GB接入参数
typedef struct VissGbParam
{
	char sServerID[32];    	//SIP接入服务ID
	char sServerIP[32];    	//SIP接入服务IP
	char sServerDNS[32];    //SIP接入服务器DNS
	int iServerPort;     	//SIP接入服务端口
	char sDevID[32];     	//设备ID
	char sDevRegPwd[36]; 	//设备注册密码
	int iDevSipPort;     	//设备本地sip端口
	int iRegVaildTime;   	//注册有效期
	int iHeatBeatTime;  	//心跳周期
	int sipMode;        	//0:udp 1:tcp
	int iEnBidirectauth;  	//双向认证使能：0：关闭、1：打开、2自动检测
	int chnNo;				//媒体流通道数
	char Cameraid[32][32];  //通道ID
	char Alarmid[32];  		//警告ID
	int CameraLevel;		//通道级别
	int AlarmLevel;			//警告级别
}VissGbParam;

#endif
