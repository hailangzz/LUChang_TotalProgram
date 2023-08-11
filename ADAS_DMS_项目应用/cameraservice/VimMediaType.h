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
	AV_FRAME_FMT sampleFmt;	//��Ƶ����λ��
	int sampleRate;			//��Ƶ������
	int channel;			//��Ƶͨ����
}AFrameInfo;

typedef struct
{
	AV_FRAME_FMT pixelFmt;
	int width;				//��Ƶ֡��
	int height;				//��Ƶ֡��
	int frameRate;			//֡��
}VFrameInfo;

typedef enum
{
	AV_CB_EVENT_DATA = 0, 	//AV���ݰ�
	AV_CB_EVENT_SEEK, 		//AV���ݰ�seek,����demuxer���õ����¼�
	AV_CB_EVNET_ERROR,		//ý��Դ����
	AV_CB_EVENT_END      	//ý��Դ����
}AV_CB_EVENT;

//����ͨ��0~7
#define MIN_VENC_CH 0
#define MAX_VENC_CH 7
//¼��ͨ��0~15
#define MIN_REC_CH 0
#define MAX_REC_CH 15
//¼��ʱ��10s~3600s
#define MIN_REC_TIMES 10
#define MAX_REC_TIMES 3600

typedef int (*VencPacketCb)(int chId, AV_CODEC_FMT fmt, unsigned char* data, int len, long long pts, long long dts, int flag, void* userData);
typedef int (*JpegPacketCb)(int chId, unsigned char* data, int len, int w, int h, long long pts, void* userData);

//��Ƶ����ص����ӿ��ﲻ�ܳ�ʱ������
typedef int (*AudioFrameCb)(const AFrameInfo* info, const unsigned char* data, int len, long long pts, void* userData);

//¼����ƵԴ����
typedef enum 
{
	PUSH_FRAME_TYPE = 0, 	//�û��ֶ�����yuvframe����
	BIND_VPSS_TYPE,			//¼��ͨ���Զ���VPSS
}RECORD_SOURCE;

//vimģ�����ID��ͨ��ID
typedef struct
{
	int devId;
	int chId;
}VimModule;

typedef struct
{
	double duration;	//¼��ʱ������λΪ�룬��Ϊ����¼�����ʱ��ΧΪ10~3600
	int bitRate;		//���ʣ���λΪkbps
	AV_CODEC_FMT vFmt;	//��Ƶ�����ʽ��Ŀǰ��֧�֣�AV_FMT_H265
	AV_CODEC_FMT aFmt;	//��Ƶ�����ʽ��Ŀǰ��֧�֣�AV_FMT_AAC
    VFrameInfo vInfo;	//��Ƶͨ����Ϣ
	AFrameInfo aInfo;	//��Ƶͨ����Ϣ
	char vEncConfig;	//���ܲ���(Ŀǰֻ֧����Ƶsvac2���ܺ���Ƶaac����)��vEncConfig��bitλ��һλ��ʾ��Ƶ�Ƿ���ܣ�����Ϊ��ʾ��Ƶ�Ƿ����
						//Ŀǰ֧�ֵ�ֵ������Ƶ�������ܣ�0x0,����Ƶ�����ܣ�0x11,��Ƶ������Ƶ�����ܣ�0x01
}MediaInfo;

typedef struct
{
	AV_TYPE type;
	AV_CODEC_FMT fmt;
	long long pts;
	long long dts;
	unsigned char *data;
	unsigned int len;
	char flags; //�ؼ�֡��־
}MpsPacket;

/**
 * @brief ��̬��ȡ¼��ȫ·���������ļ���(��.mp4��β)��Ŀ¼���д���
 * @param ch �����������Ӧ¼��ͨ��ID
 * @param event ���������0λ��ʱ¼�񣬴���0��Ӧ�¼�¼���eventֵ
 * @param fullpath �������������¼��ȫ·�������Ȳ��ܳ���1024������Ϊδ�������
 * @note �û���Ҫ��֤�˽ӿڲ������������ҷ��ص�¼��·�����ظ�
 * @return �ɹ���0�������룺����
 */
typedef int (*GetRecFullPathCb)(int ch, int event, char* fullpath);

//player��Ƶ���ģ��ͨ����Ϣ
typedef struct
{
	int type;			//ģ�����ͣ�Ŀǰ��֧��0��vpssģ�顢1��voģ��
	int devId;			//ģ���豸��
    int chId;			//ģ��ͨ����
    AV_FRAME_FMT vFmt;	//ģ���������Ƶ��ʽ��PIXEL_FMT_I420:yuv420p��PIXEL_FMT_NV12:yuv420sp
}VoModInfo;

/**
 * @brief ����״̬�ص��ӿ�
 * @param handle ���ž��
 * @param event ����״̬�¼�
 * @param data ״̬���ݣ���eventֵΪPLAY_PROG_EVNETʱ���β�����Ч��Ϊdouble����ָ�룬Ϊ��ǰ����ʱ���
 * @param userData �û�����
 * @note
 * @return �ɹ���0�������룺����
 */
typedef int(*PlayCallBack)(int handle, int event, void*data, void*userData);

//¼�񲥷�״̬�¼�
typedef enum 
{
	PLAY_START_EVNET = 0,	//���ſ�ʼ
	PLAY_PROG_EVNET,		//���Ž���
	PLAY_OVER_EVENT,		//���Ž���
	PLAY_ERROR_EVENT,		//���ų���
};

//���ſ�������
typedef enum 
{
	PLAY_PAUSE_CMD = 0,	//��ͣ����
	PLAY_START_CMD,		//��ͣ���������
	PLAY_SEEK_CMD,		//������ת
	PLAY_SPEED_CMD,		//�����ٶȿ��ƣ�ֻ֧�ֱ��ٲ���
};

// rtsp�¼��ص��ṹ��
typedef struct RtspEventHandler
{
	int (*OnStart)();//����0��ʾ����rtsp���󣬷���ܾ�����,������������ͬʱ���ӵ�rtsp�ͻ��˸���
	void (*OnStop)();
} RtspEventHandler;

// �ⲿ��д�ӿڽṹ��
typedef struct ExtRwHanlder
{
	//�ӿڲ����ͷ���ֵ��Ҫ����C�ļ���д�ӿڣ����нӿڶ�Ҫʵ��
	void *(*open)(const char *fileName, const char *modes);
	size_t (*read)(void *param, size_t size, size_t n, void *stream);
	size_t (*write)(const void *ptr, size_t size, size_t n, void *stream);
	int (*seek)(void *stream, __off64_t off, int whence);
	__off64_t (*tell)(void *stream);
	int (*close)(void *stream);
} ExtRwHanlder;

//vissƽ̨GB�������
typedef struct VissGbParam
{
	char sServerID[32];    	//SIP�������ID
	char sServerIP[32];    	//SIP�������IP
	char sServerDNS[32];    //SIP���������DNS
	int iServerPort;     	//SIP�������˿�
	char sDevID[32];     	//�豸ID
	char sDevRegPwd[36]; 	//�豸ע������
	int iDevSipPort;     	//�豸����sip�˿�
	int iRegVaildTime;   	//ע����Ч��
	int iHeatBeatTime;  	//��������
	int sipMode;        	//0:udp 1:tcp
	int iEnBidirectauth;  	//˫����֤ʹ�ܣ�0���رա�1���򿪡�2�Զ����
	int chnNo;				//ý����ͨ����
	char Cameraid[32][32];  //ͨ��ID
	char Alarmid[32];  		//����ID
	int CameraLevel;		//ͨ������
	int AlarmLevel;			//���漶��
}VissGbParam;

#endif
