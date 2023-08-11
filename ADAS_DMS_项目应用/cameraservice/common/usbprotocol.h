#ifndef __USBPROTOCOL_H__
#define __USBPROTOCOL_H__


#define __USBPROTOCOL_H_VER__ 0x01000022
#if defined(__GNUC__)
#ifndef DWORD
typedef unsigned int DWORD;
#endif
//#include <sys/types.h>
#endif
#pragma pack(1)

#define USBP_VERSION 1
typedef enum _bulk_payload_type {
	VM_USB_DEC_COMPRESSEDFRAME		= 0x10,
	VM_USB_DEC_UNCOMPRESSEDFRAME	= 0x11,
	VM_USB_VIDEOCTRL				= 0x12,
	VM_USB_SEC						= 0x13,
	VM_USB_DEC_AUDIO				= 0x14,
	VM_USB_CONFIG					= 0x15,
	VM_USB_INTERRUPT_TLV			= 0x20,
	VM_USB_VENDOR_1					= 0x1000,
}bulk_payload_type;

typedef struct _bulktrans_header {
	unsigned int dwtype;			//must be 0xFAFAFAFA
	unsigned int ver;				//protocol version
	unsigned int payload_type;		//bulk_payload_type
	unsigned int len;
	unsigned int data_offset;
	unsigned int res[3];
}bulktrans_header;
//Payload Header
//probe
typedef enum {
	USB_VIDEO_DEFAULT = 0,
	USB_VIDEO_SVAC = 1,
	USB_VIDEO_SVAC2_8BIT = 2,		//8bit
	USB_VIDEO_SVAC2_10BIT = 3,		//10bit
	USB_VIDEO_H265 = 0x10,
	USB_VIDEO_YUV420P_8BIT = 0x20,
	USB_VIDEO_YUV422P_8BIT = 0x21,
	USB_VIDEO_YUV420P_10BIT = 0x30,
}USB_VIDEO_FORMAT;
typedef struct _bulk_probe_commit {
	unsigned int channel_id;
	unsigned char bmode;			//0 ->decode, 1->encode
	unsigned int src_format;			//USB_VIDEO_FORMAT定义的格式
	unsigned int tar_format;			//0-按码流实际格式 
	unsigned int src_width;				
	unsigned int src_height;		
	unsigned char bscale;			//1 ->scale
	unsigned int tar_width;
	unsigned int tar_height;
}bulk_probe_commit;
typedef struct _bulk_commit_ack {
	int channel_id;
	int nstatus;				//0 成功
}bulk_commit_ack;
//TLV
typedef struct _usb_tlv {
	unsigned int tag;
	unsigned int len;
	char val[0];
}usb_tlv;




//Decode 

#define ATTACK_DEFENCE_SIM     1
#define EXT_SEC_INFO           1
#define MULTI_CH               1




typedef struct
{
	int    roi_info;            //ROI 区域信息,保留
	int    top;                 //ROI 顶边坐标 [y_min] //16像素单位，
	int    left;                //ROI 左边坐标 [x_min]
	int    bot;                 //ROI 底边坐标 [y_max]
	int    right;        //ROI 右边坐标 [x_max]
}USB_SVAC_ROI;

typedef struct
{
	unsigned int  extension_id; //扩展类型ID
	unsigned int  extension_length; //在extension_length之后的字节长度 
	unsigned int  reserved_offset;//该扩展单元的内容偏移，从bulk_in_dec.data[0]开始
}USB_SVAC_EXT_RESERVED_DATA;

typedef struct
{
	int  osd_type;  //信息类型，32－时间；33－摄像机名称，34－地点标注
	int  code_type; //编码格式，0－UTF-8编码
	int  align_type; //对齐格式，0－左对齐；1－右对齐
	int  char_size; //字体大小
	int  char_type; //字体格式，0－白底黑边；1－黑底白边；2－白色；3－黑色；4－自动反色
	int  osd_top; //字符上边界在画面中的坐标
	int  osd_left; //字符左边界在画面中的坐标
	int  osd_data_len; //OSD数据字节长度
	unsigned int  osd_offset;//OSD数据偏移，从bulk_in_dec.data[0]开始

}USB_SVAC_OSD;



typedef struct
{
	int   analysis_id; // 分析结果的id
	int   description_type; //描述形式
	int   analysis_data_len; //分析结果的字节长度
	unsigned int analysis_offset;	//分析结果数据偏移，从bulk_in_dec.data[0]开始

}USB_SVAC_AI_DATA;

typedef struct
{
	int  TimeEnable;                  //是否包含时间信息
	int  SurveilEnable;               //是否包含监控信息
	int  Surveil_alert_flag;          //是否包含报警信息
	int  RoiEnable;                   //图像是否包含ROI信息

	//时间信息
	int  time_year;                   //年
	int  time_month;                  //月
	int  time_day;                    //日
	int  time_hour;                   //时   
	int  time_minute;                 //分   
	int  time_second;                 //秒
	int  time_fractional;             //毫秒
	int  time_ref_date_flag;          //为1表示包括年月日的信息
	int  FrameNum;                    //时间对应图像帧的frame_num，I帧为0，其他帧逐帧加1

	//扩展事件信息    
	int  Surveil_event_num[16]; //扩展事件数
	int  Surveil_event_ID[16]; //扩展事件ID

	//报警事件信息    
	int  Surveil_alert_num;                  //报警事件数目
	int  Surveil_alert_ID[16];        //报警事件信息       
	int  Surveil_alert_region[16];           //位置编号信息
	int  Surveil_alert_frame_num[16]; //摄像头编号信息

	//ROI信息            
	int           RoiNum;                           //图像包含ROI数目
	int           RoiSkipMode;               //ROI背景跳过模式
	int           Roi_SVC_SkipMode;    //ROI_SVC背景跳过模式       
	USB_SVAC_ROI      RoiRect[15];               //基本层ROI矩形信息         
	USB_SVAC_ROI      Roi_SVC_ElRect[15];  //SVC增强层ROI矩形信息，SVC下有效

	//用户自定义扩展单元内容，解码库返回该扩展单元的全部内容
	unsigned int   unit_num; //自定义扩展单元数量
	unsigned int   ExtEnableFlag[8]; //每一个标志位表示自定义扩展信息ID是否存在返回值中。如第7个比特位为0表示extension_id为7的自定义扩展数据内容不存在，值为1则表示存在
	USB_SVAC_EXT_RESERVED_DATA   ext_reserved_data[32];

	int  AIinfoEnable;         //是否包含智能分析信息
	int    OSDEnable;           //是否包含OSD扩展信息
	int  GInfoEnable;          //是否包含地理信息

	//智能分析信息
	int   analysis_num; //分析结果的数目
	USB_SVAC_AI_DATA analysis_info[64];

	//OSD信息
	int    osd_num; //OSD信息数目
	USB_SVAC_OSD osd_info[8];

	//地理信息
	int  longitude_type; //0-东经；1-西经
	int  longitude_degree; //经度度数
	int  longitude_fraction; //经度度数的分数部分
	int  latitude_type; //0-北纬；1-南纬
	int  latitude_degree; //纬度度数
	int  latitude_fraction; // 纬度度数的分数部分
	int  gis_height; //高度，以米为单位,
	int  gis_speed; //速度，以米/秒为单位
	int  yaw_degree; //方向角，0－正北；其他－从正北按顺时针递增

	//保留数据区
	int                  Reserved[128];

}USB_SVAC_EXT_DATA;

#ifndef DEFINE_CHECK_AUTHENTICATION_RESULT
#define DEFINE_CHECK_AUTHENTICATION_RESULT
typedef struct
{
	int	authentication_result_bl;		//基本层验签结果：0-失败，1-成功，2-没有签名数据，3-没有验签结果
	int	authentication_result_el;		//增强层验签结果：0-失败，1-成功，2-没有签名数据，3-没有验签结果
	int  FrameNum;		//验签结果对应帧的frame_num，I帧为0，其他帧逐帧加1
	char  camera_id[20];			//视频来源摄像机ID
} CHECK_AUTHENTICATION_RESULT;
#endif

typedef struct
{
	int channel_id;			//解码通道号
	int length;           //整个结构，包含末尾数组数据总长
	int           nIsEffect;                        //bit0（最低位）：本解码输出结构体中图像数据是否有效，0-无效，1-有效；bit1：本解码输出结构体中扩展信息数据是否有效，0-无效，1-有效；bit2：本解码输出结构体中验签数据是否有效，0-无效，1-有效；
						//bit3：本解码输出结构体中解密码流数据是否有效，0-无效，1-有效；
						//Bit4：5：00 本码流解码没有使能空域，只有一个yuv图像；
						//01 本码流解码yuv图像是基本层；
						//10 本码流解码yuv图像是增强层；

	/* 图像相关数据*/
	int Y_offset;						//Y通道数据偏移，从data[0]计算
	int U_offset;						//U通道数据偏移，从data[0]计算
	int V_offset;						//V通道数据偏移，从data[0]计算
	int YSVC_offset;					//SVC增强层解码后y分量数据偏移，从data[0]计算
	int USVC_offset;					//SVC增强层解码后u分量数据偏移，从data[0]计算
	int VSVC_offset;					//SVC增强层解码后v分量数据偏移，从data[0]计算
	int stream_offset;					//脱密后的码流数据偏移量，从data[0]计算
	int stream_len;						//脱密后的码流长度
	int  frameType;  // 0:I帧  1:P帧  2:B帧
	int           nWidth;                                //帧宽
	int           nHeight;                               //帧高
	int			  nWidthStride;           //帧宽对齐无效像素
	int			  nHeightStride;          //帧高对齐无效像素

	int           nWidth_EL;                         //SVC增强层帧宽
	int           nHeight_EL;                        //SVC增强层帧高
	int			  nWidthStride_EL;        //增强层帧宽对齐无效像素
	int			  nHeightStride_EL;       //增强层帧高对齐无效像素

	int  chroma_format_idc; //解码输出的YUV格式
	//int       SVC_flag;                             // 码流是否为SVC模式，0-否，1-是
	int spatial_svc_flag;
	int           SVAC_STATE;     // 最低位(0)代表SVC增强层图像是否输出，0-否，1-是
	int           luma_bitdepth;                // 表示亮度的比特深度
	int           chroma_bitdepth;           // 表示色度的比特深度
	int  FrameNum;                    //图像数据对应帧的frame_num，I帧为0，其他帧逐帧加1
	
	/*扩展信息数据*/
	USB_SVAC_EXT_DATA   DecExtData;  // 扩展信息
	/*验签结果*/
	CHECK_AUTHENTICATION_RESULT  CheckAuthData;//验签结果

	unsigned int pDecrypStreamL;  //解密码流输出地址低32bit，
	unsigned int pDecrypStreamH;  //解密码流输出地址高32bit，（64bit编译时使用）
	int decryp_buff_length; //解密码流buff的大小，
	int decryp_data_length; //解密码流实际数据的大小，（该值应不大于decryp_buff_length）
#if ATTACK_DEFENCE_SIM
				/* 图像安全信息 */
	unsigned char pic_authen_idc; // 图像数据是否进行认证，0-否，1-是
	unsigned char pic_encryp_idc; // 图像数据是否进行加密，0-否，1-是
	unsigned char ext_authen_idc; // 扩展信息是否进行认证，0-否，1-是
	unsigned char ext_encryp_idc; // 扩展信息是否进行加密，0-否，1-是

	/* 安全参数集信息 */
	int encrptFlag; // 是否开启加密，0-否，1-是
	int authFlag;  // 是否开启认证，0-否，1-是
	unsigned char  certID_data[20]; //19 // 证书ID
	unsigned char  camID_data[20];   // 设备ID
#if EXT_SEC_INFO
	unsigned int cert_state;  // 证书状态，取值为VSEC_CERT_STATE
	int frame_authen_type; // 0-有验签结果的帧；1-无验签结果的帧；2-不参与验签的帧，
	int isDecWrong;                                // 是否解密错;取值为VSEC_PWUK_STATE 
#endif
	int  Reserved[180];     //解码输出保留数据扩展区
#else
	int  Reserved[196];//解码输出保留数据扩展区
#endif
	char data[0];                      //所有图像数据和码流数据

} bulk_in_dec;


typedef struct
{
	int channel_id;							//解码通道号
	int video_offset;                      //码流数据偏移，从data[0]计算
	int video_length;					   //码流数据长度
	int chroma_format_idc;	//解码后输出的YUV格式。0- 4:0:0  1- 4:2:0  2- 4:2:2
	int  bSvcdec;		//当码流中有增强层数据时，是否解码增强层，0-否，1-是
	int  bExtdecOnly; 	//是否只解码码流中的扩展信息，0-否，1-是
	int  check_authentication_flag;//该帧数据是否需验签，0-不验签，1-只对I帧验签，2-逐帧验签

	int  bTsvcdec;  // 当码流存在时域svc增强层数据时，是否只解第零解码增强层，0-否；1-是，
	int  dec_type; // 0-解码器内部自行决定使用svac1,或者svac2解码; 1-外部指定为svac1 dec; 2-外部指定为svac2 dec,0x10为265
//	int  bAuthenOnly;    //是否对码流进行认证，0-否，1-是
//	int  instance_type;  //当前解码例程的种类， 0-解码例程，1-获取码流信息例程

	int  bDecrypOnly;  // 是否只解密并输出解密码流，0-否，1-是
	unsigned int decryp_type; // 32bit，bit0表明图像数据是否解密，bit1表明扩展信息是否解密。
	int nwidth_src;		//源码流尺寸宽
	int nheight_src;		//源码流尺寸高
	int  Reserved[14]; 	//保留扩展参数区

	char data[0]; 
} bulk_out_dec;





//typedef struct _VT_FRAME_DESCRIPTOR {
//	UCHAR	bLength;
//	UCHAR	bDescriptorType;
//	UCHAR	bFormatIndex;			//媒体格式
//	UCHAR	bFrameIndex;			//帧格式索引
//	USHORT	wWidth;		//宽
//	USHORT	wHeight;		//高
//}VT_FRAME_DESCRIPTOR



//interrupt///////////////////////////////////////////////////////////
typedef enum _interrupt_tag {
	VM_USB_DEVSTATUS				= 0x5010,
	VM_USB_BULKIN_DATA_LEN = 0x5011,
	VM_USB_BUFFER_THRESHOLD = 0x5012,
	//CHANNEL_PREFETCH		= 0x12,	
};
typedef struct _interrupt_bulkin_data_len{
	unsigned int bulkin_num;				//bulk in pipe number, 0 or 1
	unsigned int data_len;					//the length of the data to be read
	unsigned int chn_id;
}interrupt_bulkin_data_len;

typedef struct _interrupt_bulkin_buf_threshold {
	unsigned int bulkin_num;				//bulk in pipe number, 0 or 1
	unsigned int chn_id;
	unsigned int vpubuffer_size;			//vpu buffer分配的实际大小
	unsigned int threshold;					//设置阈值的实际大小
	unsigned int buffer_status;				//当前vpu buffer占用的大小
}interrupt_bulkin_buf_threshold;
//typedef struct _interrupt_bulkin_data_len {
//	unsigned int bulkin_num;				//bulk in pipe number, 0 or 1
//									//the length of the data to be read
//	unsigned int chn_id;
//	unsigned int data_num;
//	unsigned int data_len[5];
//}interrupt_bulkin_data_len;

typedef struct _interrupt_channel_status {
	unsigned int channel_id;						//decode channel number
	unsigned int status;							//status
}interrupt_channel_status;

typedef struct _interrupt_sec {

}interrupt_sec;

typedef struct _interrupt_trans_data {
	unsigned int dwtype;				//must be 0xFAFAFAFA
	unsigned int ver;				//protocol version
	unsigned int payload_type;			//bulk_payload_type
	unsigned int len;				//有效的数据长度
	unsigned int res[4];	
}interrupt_trans_data;


#ifndef DEFINE_SVAC_PREFETCH_PARAM
#define DEFINE_SVAC_PREFETCH_PARAM
typedef struct
{
	int	width;	//图像宽度；ssvc时为增强层宽度，
	int	height;	//图像高度；ssvc时为增强层高度，
	int	roi_flag;	//是否有ROI，0-否，1-是
	int	spatial_svc_flag;	//是否有空域SVC，0-否，1-是
	int	chroma_format_idc; 	//YUV图像格式
	int	bit_depth_luma;		//Y分量比特精度
	int	bit_depth_chroma;		//UV分量比特精度
	int svac_version;     // 1: svac1;2: svac2,
	int temporal_svc_flag; //是否有时域SVC，0-否，1-是
	int spatial_svc_ratio; // 0 – 增强层与基本层的缩放比例为4:3; 1 – 比例为2:1; 2 – 比例
							//为4 : 1; 3 – 比例为6 : 1; 4 – 比例为8 : 1
	int frame_rate; //帧率
	int  reserved[5];

} SVAC_PREFETCH_PARAM;
#endif //DEFINE_SVAC_PREFETCH_PARAM

typedef struct _stream_prefetch {
	unsigned int channel_id;		//通道号
	unsigned int data_len;			//码流数据长度
	unsigned int type;				//// 0-解码器内部自行决定使用svac1,或者svac2解码; 1-外部指定为svac1 dec; 2-外部指定为svac2 dec,0x10为265
	char data[0];					//码流数据,长度为data_len
}stream_prefetch;
typedef struct _stream_prefetch_ack {
	unsigned int channel_id;
	SVAC_PREFETCH_PARAM param;
}stream_prefetch_ack;




/****************CTRL TLV************************/
typedef enum {
	VM_USB_PROBE = 0x10,
	VM_USB_COMMIT = 0x11,
	VM_USB_STOP = 0x12,
	VM_USB_PREFETCH_OUT = 0x20,
	VM_USB_PREFETCH_IN = 0x21,
	VM_USB_COMMIT_ACK = 0x22,
	VM_USB_CTRL_RESETCHN = 0x1001,
}video_ctrl_cmd;

typedef struct _ctrl_reset_channel
{
	unsigned int channel_id;	//bit 0 -->channel 0, bit 1-->channel
	unsigned int status;		//按位对应通道号，0表示成功，1表示失败
}ctrl_reset_channel;
/****************CTRL TLV************************/

/****************CFG TLV************************/
typedef enum  {
	VM_USB_GETDEVINFO=0x0A00,
	VM_USB_GETDEVINFO_ACK=0x0A01,
	VM_USB_GETLOG=0x0A02,
	VM_USB_GETLOG_ACK = 0x0A03,

}cfg_cmd;
typedef struct {
	unsigned int nstatus;		//0 成功
	unsigned int version[4];
}usb_dev_info_ack;

typedef struct {
	unsigned int len;		// log length
	char data[0];
}usb_dev_log_ack;
/****************CFG TLV************************/


/*****************Audio***************************/
typedef struct {
	int chn_id;				//解码通道号
	int data_len;			//音频数据长度
	int data_type;			//0-->svac ,1->g711
	char data[0];
}audio_dec_out;
typedef struct {
	int chn_id;
	int data_type;			//0-->pcm
	int nStatus;			//解码状态,0为成功
	int data_len;			//pcm长度
	char data[0];
}audio_dec_ack;
/*****************Audio End***********************/
/****************SEC******************************/
#define USBSEC_NAME_LEN 128
#define	USBSEC_SN_LEN 64
#define USBSEC_MAX_CERT_NUM 16
#define USBSEC_UKEY_CERT_LEN 4096
typedef enum _sec_cmd_tag {
	VM_SEC_INIT				=0xA000,
	VM_SEC_INIT_ACK			=0xA001,
	VM_SEC_CLOSE			=0xA002,
	VM_SEC_CLOSE_ACK		=0xA003,
	VM_SEC_GETDEVINFO		=0xA004,
	VM_SEC_GETDEVINFO_ACK	=0xA005,
	VM_SEC_GETCAPS			=0xA006,
	VM_SEC_GETCAPS_ACK		=0xA007,
	VM_SEC_GETR1			=0xA008,
	VM_SEC_GETR1_ACK		=0xA009,
	VM_SEC_GETSIGN1			=0xA00A,
	VM_SEC_GETSIGN1_ACK		=0xA00B,
	VM_SEC_VEFYSIGN1		=0xA00C,
	VM_SEC_VEFYSIGN1_ACK	=0xA00D,
	VM_SEC_VEFYSIGN2		=0xA00E,
	VM_SEC_VECFYSIGN2_ACK	=0xA00F,
	VM_SEC_SIPAUTH			=0xA010,
	VM_SEC_SIPAUTH_ACK		=0xA011,
	VM_SEC_SIPVERIFY		=0xA012,
	VM_SEC_SIPVERIFY_ACK	=0xA013,
	VM_SEC_VKEKTRANS		=0xA014,
	VM_SEC_VKEKTRANS_ACK	=0xA015,
	VM_SEC_GETCSR			=0xA016,
	VM_SEC_GETCSR_ACK		=0xA017,
	VM_SEC_IMPORTCERT		=0xA018,
	VM_SEC_IMPRORTCERT_ACK	=0xA019,
	VM_SEC_IMPORTECCKEY		=0xA01A,
	VM_SEC_IMPORTECCKEY_ACK	=0xA01B,
	VM_SEC_EXPORTCERT		=0xA01C,
	VM_SEC_EXPORTCERT_ACK	=0xA01D,
	VM_SEC_SETVKEK			=0xA01E,
	VM_SEC_SETVKEK_ACK		=0xA01F,
	VM_SEC_VEFY_UKEY		= 0xA020,
	VM_SEC_VEFY_UKEY_ACK	= 0xA021,
	VM_SEC_GET_ENCCERT		= 0xA022,
	VM_SEC_GET_ENCCERT_ACK	= 0xA023,
	VM_SEC_GET_CERT			= 0xA024,
	VM_SEC_GET_CERT_ACK		= 0xA025,
	VM_SEC_SET_DECCERT		= 0xA026,
	VM_SEC_SET_DECCERT_ACK	= 0xA027,
	VM_SEC_SET_DECENCCERT	= 0xA028,
	VM_SEC_SET_DECENCCERT_ACK = 0xA029,
	VM_ATTACHDEV			=0xA02A,
	VM_ATTACHDEV_ACK		=0xA02B,
	VM_SEC_ENUM_CERT		= 0xA02C,
	VM_SEC_ENUM_CERT_ACK	= 0xA02D,
	VM_SEC_REENCRPTEVEK		=0xA02E,
	VM_SEC_REENCRPTEVEK_ACK =0xA02F,
	VM_SEC_SETPLAINKEY		=0xA030,
	VM_SEC_SETPLAINKEY_ACK  =0xA031,
	VM_SEC_GETUKEYINFO		= 0xA032,
	VM_SEC_GETUKEYINFO_ACK	= 0xA033,
	VM_SEC_PWDENC			= 0xA034,
	VM_SEC_PWDENC_ACK		= 0xA035,
	VM_SEC_CHANGEDEVPIN		= 0xA036,
	VM_SEC_CHANGEDEVPIN_ACK = 0xA037,
	VM_SEC_SETDEVUSERNAME	= 0xA038,
	VM_SEC_SETDEVUSERNAME_ACK = 0xA039,
	VM_SEC_RESTDEVPIN		= 0xA03A,
	VM_SEC_RESTDEVPIN_ACK	= 0xA03B,
	VM_SEC_SETCERTCHAIN		= 0xA03C,
	VM_SEC_SETCERTCHAIN_ACK = 0xA03D,
	VM_SEC_EXTERNALDATACIPHER		= 0xA03E,
	VM_SEC_EXTERNALDATACIPHER_ACK	= 0xA03F,
	VM_SEC_GETDEVUSERNAME			= 0xA040,
	VM_SEC_GETDEVUSERNAME_ACK		= 0xA041,
	VM_SEC_GETSTRMINFO				=0xA042,
	VM_SEC_GETSTRMINFO_ACK			=0xA043,
	VM_SEC_GETCERTSTAT				=0xA044,
	VM_SEC_GETCERTSTAT_ACK			=0xA045,
}sec_cmd_tag;     
typedef struct {
	unsigned int sec_chn_id;	//安全通道号
	unsigned int len;			//数据长度
	char data[0];				//长度为len的数据
}sec_elem;
typedef struct Struct_Version{
 char major;      // 主版本号
 char minor;      // 次版本号
}USB_VERSION;
typedef struct {
	unsigned int sec_chn_id;	//安全通道号
	unsigned int mode;			//设备工作模式 0->发证，1->双向认证，2->码流解密
}sec_init;

typedef struct
{
	unsigned int type;                  // viss or IE证书
	unsigned int certType;				//ukey证书类型 0: ECC
	unsigned int keyUsage;              // 0：加密证书；1：签名证书
	unsigned char appName[USBSEC_NAME_LEN];    // app名称
	unsigned char ctnName[USBSEC_NAME_LEN];    // ctn名称 
	unsigned char  certID[USBSEC_NAME_LEN];
	unsigned char  certFrom[USBSEC_SN_LEN];			// ukey内证书颁发者
	unsigned char  certUser[USBSEC_SN_LEN];			// ukey内证书使用者
	unsigned char  certDateFrom[USBSEC_SN_LEN];		//ukey证书颁发日期
	unsigned char  certDateTo[USBSEC_SN_LEN];			//ukey证书截至日期
	unsigned char certData[USBSEC_UKEY_CERT_LEN];
	unsigned int certlen;
	unsigned char res[504];
} usbkey_cert_info;
typedef struct
{
	//转换
	unsigned char mfrsID[4];            // 设备ID编号，解码棒内安全芯片ID
										// PC端不上报该值，此值仅用于PC端sdk和解码棒交互
	unsigned char devName[USBSEC_NAME_LEN];    // 枚举名称
	unsigned char devSN[USBSEC_SN_LEN];    // 设备序列号
	unsigned int certNum;               // 证书个数
	usbkey_cert_info certInfo[USBSEC_MAX_CERT_NUM];   // 证书信息
} usbkey_search_result;

typedef struct {
	unsigned int sec_chn_id;	//安全通道
	USB_VERSION		Version;			// 版本号,设置为1.0
	char		Manufacturer[64];	// 设备厂商信息,以'\0'为结束符的ASCII字符串
	char		Issuer[64];			// 发行厂商信息,以'\0'为结束符的ASCII字符串
	char		Label[32];			// 设备标签,以'\0'为结束符的ASCII字符串
	char		SerialNumber[32];	// 序列号,以'\0'为结束符的ASCII字符串
	USB_VERSION		HWVersion;			// 设备硬件版本
	USB_VERSION		FirmwareVersion;	// 设备本身固件版本
	unsigned int		AlgSymCap;			// 分组密码算法标识
	unsigned int		AlgAsymCap;			// 非对称密码算法标识
	unsigned int		AlgHashCap;			// 密码杂凑算法标识
	unsigned int		DevAuthAlgId;		// 设备认证的分组密码算法标识
	unsigned int		TotalSpace;			// 设备总空间大小
	unsigned int		FreeSpace;			// 用户可用空间大小
	unsigned int		MaxECCBufferSize;	// 能够处理的ECC加密数据大小 
	unsigned int		MaxBufferSize;      // 能够处理的分组运算和杂凑运算的数据大小
	char		Reserved[64];		// 保留扩展				
}sec_devinfo;



typedef struct {
	unsigned int sec_chn_id;	//安全通道
	unsigned int ACaps;                      //非对称算法描述
	unsigned int HCaps;                      //杂凑算法描述
	unsigned int SCaps;                      //对称算法描述
	unsigned int SICaps;                     //签名算法描述			
}sec_caps;

//typedef struct
//{
//	u8 *data;
//	int len;
//} VSEC_ELEM, *PVSEC_ELEM;


typedef struct {
	unsigned int sec_chn_id;	//安全通道号
	unsigned int svrID_offset;
	unsigned int svrID_len;
	unsigned int r1_offset;
	unsigned int r1_len;	
	char data[0];			
}sec_sign1_param;

typedef struct {
	unsigned int sec_chn_id;	//安全通道号
	unsigned int r2_offset;
	unsigned int r2_len;
	unsigned int sign1_offset;
	unsigned int sign1_len;	
	char data[0];			
}sec_sign1_ack_param;


typedef struct {
	unsigned int sec_chn_id;	//安全通道号
	unsigned int r1_len;
	char data[0];
}sec_getr1_ack;

typedef struct
{
	unsigned int sec_chn_id;
	
	// verify sign1
	unsigned int r1_offset;
	unsigned int r1_len;
	unsigned int r2_offset;
	unsigned int r2_len;
	unsigned int svrID_offset;
	unsigned int svrID_len;	
	unsigned int sign_offset;
	unsigned int sign_len;		
	
	// sign sign2
	unsigned int devID_offset;
	unsigned int devID_len;
	unsigned int cVkek_offset;
	unsigned int cVkek_len; 		// contain input cryptkey enc by sdk pbkey, 
						            // if len=0, creat by sdk, return cryptkey enc by sdk pbkey
	unsigned int keyVer_offset;
	unsigned int keyVer_len;		// cVkek 对应的 vkek version
    char data[0];	               
} sec_vefy_param1, *psec_vefy_param1;


typedef struct
{
	unsigned int sec_chn_id;	
	
	unsigned int cryptKey_offset;			//SM2加密后的数据，并经过base64处理
	unsigned int cryptKey_len;
	unsigned int sign2_offset;
	unsigned int sign2_len;
	
    char data[0];	               
} sec_vefy_ack_param1, *psec_vefy_ack_param1;


typedef struct
{
	unsigned int sec_chn_id;
	unsigned int r1_offset;
	unsigned int r1_len;
	unsigned int r2_offset;
	unsigned int r2_len;
	unsigned int devID_offset;
	unsigned int devID_len;	
	unsigned int sign2_offset;
	unsigned int sign2_len;

	unsigned int cryptKey_offset;
	unsigned int cryptKey_len;
	unsigned int keyVer_offset;
	unsigned int keyVer_len;	
	unsigned int srvID_offset;
	unsigned int srvID_len;		 // search server pub key
	char data[0];	
} sec_vefy_param2, *psec_vefy_param2;


typedef struct
{
	unsigned int sec_chn_id;
	
	unsigned int date_offset;
	unsigned int date_len;
	unsigned int method_offset;
	unsigned int method_len;
	unsigned int from_offset;
	unsigned int from_len;	
	unsigned int to_offset;
	unsigned int to_len;
	
	unsigned int callID_offset;
	unsigned int callID_len;
	unsigned int msg_offset;
	unsigned int msg_len;	
	unsigned int nonce_offset;
	unsigned int nonce_len;

	char data[0];
} sec_sipauth_param, *psec_sipauth_param;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int nstatus;
	unsigned int data_len;
	char data[0];
}sec_sipauth_ack;
typedef struct
{
	unsigned int sec_chn_id;
	
	unsigned int vkekVer_offset;
	unsigned int vkekVer_len;
	unsigned int cVkek_offset;
	unsigned int cVkek_len;
	//unsigned int pubkey_offset;
	//unsigned int pubkey_len;
	char data[0];	            // 标识vkek号, 唯一, 与 vkek一一对应
								// 密文形式的vkek, 用于解密vek   
} sec_vkek_param, *psec_vkek_param;

typedef struct
{
	unsigned int sec_chn_id;

	unsigned int vkekVer_offset;
	unsigned int vkekVer_len;
	unsigned int cVkek_offset;
	unsigned int cVkek_len;
	unsigned int pubkey_offset;
	unsigned int pubkey_len;
	unsigned int certID_offset;
	unsigned int certID_len;
	char data[0];	            // 标识vkek号, 唯一, 与 vkek一一对应
								// 密文形式的vkek, 用于解密vek   
} sec_vkektrans_param, * psec_vkektrans_param;

typedef struct
{
	unsigned int sec_chn_id;
	
	unsigned int vkekVer_offset;
	unsigned int vkekVer_len;
	unsigned int cVkek_offset;
	unsigned int cVkek_len;
	char data[0];	            // 标识vkek号, 唯一, 与 vkek一一对应
								// 密文形式的vkek, 用于解密vek   
} sec_vkek_ack_param, *psec_vkek_ack_param;



typedef struct
{
    unsigned int mode;		// 0:bin; 1:base64
	unsigned int sec_chn_id;
	
	unsigned int cn_offset;
	unsigned int cn_len;
	//unsigned int p10_offset;
	//unsigned int p10_len;
	unsigned int ogu_offset;
	unsigned int ogu_len;	
	unsigned int og_offset;
	unsigned int og_len;
	
	unsigned int locate_offset;
	unsigned int locate_len;
	unsigned int state_offset;
	unsigned int state_len;	
	unsigned int country_offset;
	unsigned int country_len;
	unsigned int email_offset;
	unsigned int email_len;
	unsigned int pin_offset;
	unsigned int pin_len;
	char data[0];
} sec_csr_param, *psec_csr_param;

typedef struct {
	unsigned int p10_offset;
	unsigned int p10_len;
	char data[0];
} sec_csr_param_ack;
typedef struct
{	
	unsigned int type;		// 0 for sign cert, 1 for enc cert
	unsigned int sec_chn_id;
    unsigned int cert_offset;
	unsigned int cert_len;
	char data[0];
} sec_cert_param, *psec_cert_param;

typedef struct
{
	unsigned int sec_chn_id;
	unsigned int bindid_offset;			// 说明该证书和哪个ID绑定，serID or devID
	unsigned int bindid_len;
	unsigned int certid_offset;
	unsigned int certid_len;
	unsigned int cert_offset;
	unsigned int cert_len;
	char data[0];
} sec_bd_setcert_param;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int nstatus;
	unsigned int retryCount;
} sec_vefy_ukey_ack;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int ca_id;
	unsigned int data_len;
	char data[0];
}sec_import_ecckey;
typedef struct
{
	unsigned int sec_chn_id;
	char	   mfrsID[USBSEC_NAME_LEN];				//ukey厂家类型
	char	   devName[USBSEC_NAME_LEN];				//ukey的枚举名, 也是open设备时候使用的 devName
	char	   appName[USBSEC_NAME_LEN];				//应用名，输入空采用默认应用名
	char	   cntName[USBSEC_NAME_LEN];				//容器名，输入空采用默认容器名
	char	   keyPin[USBSEC_NAME_LEN];				//ukey的pin码，输入空采用默认pin码
	char	   devSN[USBSEC_SN_LEN];				//Ukey序列号
	unsigned int   reserved[488];
} sec_attach_dev;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int mode;
	unsigned int vkekVer_offset;
	unsigned int vkekVer_len;
	unsigned int cVkek_offset;
	unsigned int cVkek_len;
	unsigned int encKey_offset;
	unsigned int encKey_len;
	unsigned int evek_offset;
	unsigned int evek_len;
	char data[0];
}sec_reencrpte_vkek;
typedef struct {
	unsigned int sec_chn_id;
	unsigned int nstatus;		//0-->successed
	unsigned int vkekVer_offset;
	unsigned int vkekVer_len;
	unsigned int evek_offset;
	unsigned int evek_len;	
	char data[0];
}sec_reencrpte_vkek_ack;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int nstatus;
}sec_gen_ack;			//


typedef struct
{
	unsigned int sec_chn_id;
	unsigned int encMode;		/* 加密模式0:对称算法;1:非对称算法 */
	unsigned int devID_offset;
	unsigned int devID_len;
	unsigned int encPW_offset;
	unsigned int encPW_len;
	unsigned int pwIn_offset;
	unsigned int pwIn_len;
	//u32 res[32];
	char data[0];
}sec_pwdenc_param;// VSEC_PWENC_PARAM;
typedef struct {
	unsigned int sec_chn_id;
	unsigned int nStatus;			//0-->successed
	unsigned int pwOut_len;
	char data[0];
}sec_pwdenc_param_ack;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int pinType;
	unsigned int oldpin_offset;
	unsigned int oldpin_len;
	unsigned int newpin_offset;
	unsigned int newpin_len;
	char data[0];
}sec_change_devpin;
typedef struct {
	unsigned int sec_chn_id;
	unsigned int nstatus;
	unsigned int changetimes;
}sec_change_devpin_ack;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int pin_offset;
	unsigned int pin_len;
	unsigned int username_offset;
	unsigned int username_len;
	char data[0];
}sec_setdevusername;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int pin_type;
}sec_reset_devpin;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int nstatus;
	unsigned int unlocktimes;
}sec_reset_devpin_ack;

typedef struct {
	unsigned int sec_chn_id;
	unsigned int bindid_offset;
	unsigned int bindid_len;
	unsigned int certid_offset;
	unsigned int certid_len;
	unsigned int cert_offset;
	unsigned int cert_len;
	char data[0];
}sec_set_certchain;

typedef struct _sec_external_cipher
{
	unsigned int sec_chn_id;
	unsigned int asymALG;    // 指定 cryptedKey 的非对称加密方式 VSEC_ASYM_ALG
	unsigned int symType;     // 加密或解密inData的算法 VSEC_ENCRYPT_TYPE
	unsigned int symMode;        // VSEC_ENCRYPT_MODE
	unsigned int mode;       // 0:解密; 1:加密
	unsigned int   cryptedKey_offset; // 非对称算法加密后的密码
	unsigned int   cryptedKey_len;
	unsigned int   iv_offset; // 根据算法需要
	unsigned int   iv_len;
	unsigned int   inData_offset; // 待处理数据
	unsigned int   inData_len;
	unsigned int res[16];
	char data[0];
}sec_external_cipher;

typedef struct _sec_strm_info
{
	unsigned int sec_chn_id;
	unsigned int encrptFlag;  //码流是否被加密 0:未加密码流        1:用户配置的密码加密 2:ukey加密
	unsigned int authFlag;  //码流是否有签名数据          0:未签名码流        1:签名码流
	unsigned int strmEncrptType;     // 码流加密算法 取值参考:VSEC_ENCRYPT_TYPE
	unsigned int strmEncrptMode;     // 码流加密模式 取值参考:     VSEC_ENCRYPT_MODE
	unsigned int vekEncrptType;          // vek加密算法 取值参考: VSEC_ENCRYPT_TYPE
	unsigned int vekEncrptMode;          // vek加密模式 取值参考: VSEC_ENCRYPT_MODE
	unsigned int hashType;       // 摘要算法 取值参考:VSEC_HASH_TYPE
	unsigned int signType;       // 签名算法 取值参考:VSEC_SIGN_TYPE
	unsigned int isDecWrong; // 是否密码错 0:解密正确;1:解密错误
	unsigned int authResult; // 认证结果 取值参考:VSEC_AUTH_RESULT
}sec_strm_info;
typedef struct _sec_getcert_info
{
	unsigned int sec_chn_id;
	unsigned int stat;
	unsigned int certid_offset;
	unsigned int certid_len;
	unsigned int camid_offset;
	unsigned int camid_len;
	char data[0];
}sec_getcert_info;
/****************SEC******************************/

#pragma pack()
#endif //__USBPROTOCOL_H__