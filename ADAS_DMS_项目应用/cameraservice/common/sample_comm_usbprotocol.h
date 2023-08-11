#ifndef __SAMPLE_COMM_USBPROTOCOL_H__
#define __SAMPLE_COMM_USBPROTOCOL_H__

#define __USBPROTOCOL_H_VER__ 0x01000024
#if defined(__GNUC__)
#ifndef DWORD
typedef unsigned int DWORD;
#endif
//#include <sys/types.h>
#endif
#pragma pack(1)

/*
* 通用 tlv
*/
typedef struct _gs_usb_tlv {
	unsigned int tag;
	unsigned int len;
	char val[0];
}gs_usb_tlv; 

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

typedef enum {
	VM_USB_PROBE                = 0x10,
	VM_USB_COMMIT               = 0x11,
	VM_USB_STOP                 = 0x12,
	VM_USB_PREFETCH_OUT         = 0x20,
	VM_USB_PREFETCH_IN          = 0x21,
	VM_USB_COMMIT_ACK           = 0x22,
	VM_USB_CTRL_RESETCHN        = 0x1001,
}gs_video_ctrl_cmd;

/*
* 中断数据（header + tlv + data）
*/
enum _interrupt_tag {
	VM_USB_DEVSTATUS			= 0x5010,
	VM_USB_BULKIN_DATA_LEN      = 0x5011,
	VM_USB_BUFFER_THRESHOLD     = 0x5012,
	//CHANNEL_PREFETCH		    = 0x12,	
};

typedef struct _gs_interrupt_trans_data {
	unsigned int dwtype;				        // must be 0xFAFAFAFA
	unsigned int ver;				            // protocol version
	unsigned int payload_type;			        // bulk_payload_type
	unsigned int len;				            // 有效的数据长度
	unsigned int res[4];	
}gs_interrupt_trans_data;

typedef struct _gs_interrupt_bulkin_data_len{
	unsigned int bulkin_num;				//bulk in pipe number, 0 or 1
	unsigned int data_len;					//the length of the data to be read
	unsigned int channel_num;				// 0 - 7 
}gs_interrupt_bulkin_data_len;

// 合并上述中断结构
typedef struct _gs_interrupt_data {
    gs_interrupt_trans_data header;
    gs_usb_tlv tlv; 
	gs_interrupt_bulkin_data_len bulkin_data;
}gs_interrupt_data;

/*
* EP2命令（header + tlv）
*/

// 30
typedef struct _gs_bulk_probe_commit {
	unsigned int channel_id;
	unsigned char bmode;   //0 ->decode, 1->encode
	unsigned int src_format;   //USB_VIDEO_FORMAT定义的格式
	unsigned int tar_format;   //0-按码流实际格式 
	unsigned int src_width;    
	unsigned int src_height;  
	unsigned char bscale;   //1 ->scale
	unsigned int tar_width;
	unsigned int tar_height;
}gs_bulk_probe_commit;
typedef struct _bulk_commit_ack {
	int channel_id;
	int nstatus;				//0 成功
}bulk_commit_ack;

// 32
typedef struct _gs_bulktrans_header {
	unsigned int dwtype;			//must be 0xFAFAFAFA
	unsigned int ver;				//protocol version
	unsigned int payload_type;		//bulk_payload_type
	unsigned int len;
	unsigned int data_offset;
	unsigned int res[3];
}gs_bulktrans_header;

typedef struct _gs_ctrl_order {
    gs_bulktrans_header header;
    gs_usb_tlv tlv; 
}gs_ctrl_order;

typedef struct _gs_ctrl_commit_ack {
    gs_bulktrans_header header;
    gs_usb_tlv tlv; 
	bulk_commit_ack ack;
}gs_ctrl_commit_ack;

/*
* 上传SVAC2码流数据格式（bulktrans_header + usb_tlv + bulk_in_dec + 码流数据）
*/
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
	int   analysis_id; // 分析结果的id
	int   description_type; //描述形式
	int   analysis_data_len; //分析结果的字节长度
	unsigned int analysis_offset;	//分析结果数据偏移，从bulk_in_dec.data[0]开始

}USB_SVAC_AI_DATA;
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

typedef struct
{
	int channel_id;		//解码通道号
	int length;         //整个结构，包含末尾数组数据总长
	int nIsEffect;      //bit0（最低位）：本解码输出结构体中图像数据是否有效，0-无效，1-有效；bit1：本解码输出结构体中扩展信息数据是否有效，0-无效，1-有效；bit2：本解码输出结构体中验签数据是否有效，0-无效，1-有效；
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
	int  frameType;                     // 0:I帧  1:P帧  2:B帧
	int           nWidth;               //帧宽
	int           nHeight;              //帧高
	int			  nWidthStride;         //帧宽对齐无效像素
	int			  nHeightStride;        //帧高对齐无效像素

	int           nWidth_EL;            //SVC增强层帧宽
	int           nHeight_EL;           //SVC增强层帧高
	int			  nWidthStride_EL;      //增强层帧宽对齐无效像素
	int			  nHeightStride_EL;     //增强层帧高对齐无效像素

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

}gs_bulk_in_dec;

// 合并上传数据结构, 948 byte
typedef struct _gs_bulk_ep1in_dec {
    gs_bulktrans_header header;
    // gs_usb_tlv tlv; 
	gs_bulk_in_dec bulkin_dec;
}gs_bulk_ep1in_dec;

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
	VM_SEC_GETCERTINFO				=0xA046,
	VM_SEC_GETCERTINFO_ACK			=0xA047,
}sec_cmd_tag;     


#pragma pack()
#endif