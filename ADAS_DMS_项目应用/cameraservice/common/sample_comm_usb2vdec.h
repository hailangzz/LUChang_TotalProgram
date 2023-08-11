#ifndef __SAMPLE_COMM_USB2VDEC_H__
#define __SAMPLE_COMM_USB2VDEC_H__

#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <signal.h>
#include <regex.h>
#include <poll.h>
#include <sys/ioctl.h>    
#include <stropts.h> 

#include <sys/mman.h>   

#include "sample_comm_vdec.h"
#include "mpi_sys.h"

#define WINUSB_EP1_DEVICE_NAME      "/dev/WinUSB_EP1"
#define WINUSB_EP2_DEVICE_NAME      "/dev/WinUSB_EP2"
#define WINUSB_EP3_DEVICE_NAME      "/dev/WinUSB_EP3"
#define WINUSB_EP1_1_DEVICE_NAME    "/dev/WinUSB_EP1_1"
#define WINUSB_EP2_1_DEVICE_NAME    "/dev/WinUSB_EP2_1"
#define WINUSB_EP3_1_DEVICE_NAME    "/dev/WinUSB_EP3_1"

#define MAX_SEND_NUMBER		8

enum eptype {
    // 对WinUSB某个端点进行操作
    EP1IN                       = 781,
    EP1OUT                      = 701,
    EP2IN                       = 782,
    EP2OUT                      = 702,
    EP3IN                       = 783,
    EP3OUT                      = 703,
    EP4IN                       = 784,
    EP4OUT                      = 704,
    EP4IN_AND_EP1IN 			= 781784,
	EP4IN_AND_EP2IN 			= 782784,
	EP4IN_AND_EP3IN 			= 783784,

    // 判断是给PC中断还是给PC数据
    VDEC_END                    = 666,
    INTERRUPT                   = 111,
    BULK_DATA                   = 999,
    INT_AND_BULK                = 111999,

    USER_TO_KERNEL_PHY 	        = 123,		// 应用层给内核传递物理地址
    RECEIVE_EP1OUT_REQUEST      = 101,
    RELEASE_EP1OUT_REQUEST      = 201,
    RECEIVE_EP2OUT_REQUEST      = 102,
    RELEASE_EP2OUT_REQUEST      = 202,
    USB_CTRL_RESETCHN	        = 0x1001,

    EP1OUT_QUEUE_REQUEST 		= 301,		// 重新入队 EP1OUT requset
	EP2OUT_QUEUE_REQUEST 		= 302,		// 重新入队 EP2OUT requset
	EP3OUT_QUEUE_REQUEST 		= 303,		// 重新入队 EP3OUT requset

    ENABLE_TIMEOUT_ON			= 1,
	ENABLE_TIMEOUT_OFF			= 0,
};

// struct _WinUSB_user_data {
// 	int A_Frame_Len;				// 一帧的长度
// 	int Ep_attribute;				// 选择端点的属性
// 	int Ep_select;					// 选择哪个端点
// 	int Frame_VirAddr;				// 一帧的虚拟地址
// 	int Frame_PhyAddr;				// 一帧的物理地址
// 	int Channel_Num;				// 通道号
// };

struct _data_dispose {
	int enable_time_out;							// 使能超时机制
	int data_timeout;								// 数据包超时时间（根据发送数据包长度来设置）
	int channel_num;								// 从软件层面知道USB层是来自哪个通道的数据
};

struct _WinUSB_user_data_new {
	int Data_Package_Size[MAX_SEND_NUMBER];			// 数据包的长度 [数组]
	int Data_Package_PhyAddr[MAX_SEND_NUMBER];		// 数据包的物理地址 [数组]
	int Data_Package_Num;							// 数据包的个数，最大 MAX_SEND_NUMBER
	int Ep_Attribute;								// 选择端点的属性(int or int+bulk)
	int Ep_Select;									// 选择哪个端点
	int Interrupt_Data_Size;						// 中断数据的长度
	int Interrupt_Data_PhyAddr;						// 中断数据的物理地址(根据协议填充)
	int Standby_Application_PhyAddr;				// 备用物理地址

	struct _data_dispose data_dispose;				// 添加额外的数据处理
};

/* 中断 */
typedef struct _interrupt_trans_data {
	unsigned int dwtype;				//must be 0xFAFAFAFA
	unsigned int ver;				//protocol version
	unsigned int payload_type;			//bulk_payload_type
	unsigned int len;				//
	unsigned int res[4];	
}interrupt_trans_data;

typedef struct _usb_tlv {
	unsigned int tag;
	unsigned int len;
	char val[0];
}usb_tlv; 

typedef struct _interrupt_bulkin_data_len{
	unsigned int bulkin_num;				//bulk in pipe number, 0 or 1
	unsigned int data_len;					//the length of the data to be read
	unsigned int channel_num;				// 0 - 7 
}interrupt_bulkin_data_len;

typedef struct _testinterrupt {
    interrupt_trans_data header;
    usb_tlv tlv; 
	interrupt_bulkin_data_len bulkin_data;
}testinterrupt;
/* 中断结束 */

//TLV
// typedef struct _usb_tlv {
// 	unsigned int tag;
// 	unsigned int len;
// 	char val[0];
// }usb_tlv;

typedef struct _bulktrans_header {
	unsigned int dwtype;			//must be 0xFAFAFAFA
	unsigned int ver;				//protocol version
	unsigned int payload_type;		//bulk_payload_type
	unsigned int len;
	unsigned int data_offset;
	unsigned int res[4];
}bulktrans_header;

typedef struct _testctrl {
    bulktrans_header header;
    usb_tlv tlv; 
}testctrl;

VIM_S32 USB_Form_Stream(SAMPLE_VDEC_CFG* testParam);
VIM_U32 USB_EP2_CMD(SAMPLE_VDEC_CFG* testParam);
VIM_S32 Dump2Usb(VDEC_CHN VdecChn, VDEC_PACK_S *sPack);
VIM_S32 InitUsbDevice();
VIM_S32 USB2VDEC_SYS_Init(void);

#endif