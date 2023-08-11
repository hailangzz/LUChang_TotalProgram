#ifndef __SAMPLE_COMM_USB2VENC_H__
#define __SAMPLE_COMM_USB2VENC_H__

#include "mpi_sys.h"
#include "sample_comm_venc.h"
#include "sample_comm_venc_ext.h"
#include <sys/prctl.h>
#include "sample_comm_usb2vdec.h"
#include "sample_comm_usbprotocol.h"

#define chn_num_max 8
#define MAX_COUNT_ROI             15
#define pthread_maxnum chn_num_max

#define ENDOINT_MAX_NUM 3			// 最大端点数

enum _WinUSB_pid {
	WinUSB_EP1OUT 			= 0,
	WinUSB_EP2OUT 			= 1,
	WinUSB_EP3OUT 			= 2,
	WinUSB_EP1IN  			= 0,
	WinUSB_EP2IN  			= 1,
	WinUSB_EP3IN  			= 2,
	WinUSB_DEV_1  			= 0,
	WinUSB_DEV_2  			= 1,
	WinUSB_DEV_3  			= 2,
	WinUSB_EP4IN_EP1_IN  	= 0,
	WinUSB_EP4IN_EP2_IN  	= 1,
	WinUSB_EP4IN_EP3_IN  	= 2,
};

struct _WinUSB1_work_sb {
	int flag_end_winusb;											// 全局退出标志，将此 flag 设置为 1 时，大部分功能将会自动退出
	int enable_commit;
	int enable_ep1out;
	int fd_mem;														// "/dev/mem" 设备句柄
	pthread_t gs_epxout_Pid;										// epxout 接收线程
	pthread_mutex_t gs_epxout_thread_mutex;
	struct pollfd epxout_fds[ENDOINT_MAX_NUM];						// 监控 ep1out ep2out ep3out 发来的数据
	int epxout_buf[ENDOINT_MAX_NUM][32];							// 保存 read 数据，通过 read 接口传递
	struct _WinUSB_user_data_new user_data_new[ENDOINT_MAX_NUM];	// 与底层 WinUSB 通信结构体，通过 write 接口传递

	gs_interrupt_data *ep4in_epxin_interrupt[ENDOINT_MAX_NUM];		// 中断结构，主要根据解码棒产品的协议
	int  ep4in_epxin_pu32PhyAddr[ENDOINT_MAX_NUM];					// ep4in_epxin_interrupt 对应的物理地址

	gs_bulk_ep1in_dec *bulk_ep1in_dec;								// bulk 传输上行数据结构，主要根据解码棒产品的协议
	int  bulk_ep1in_dec_pu32PhyAddr;								// bulk_ep1in_dec 对应的物理地址

	gs_ctrl_order *ep2_ctrl;										// 解码棒清除命令结构体
	int  ep2_ctrl_pu32PhyAddr;										// ep2_ctrl 对应的物理地址

	gs_ctrl_commit_ack *commit_ack;										// 解码棒清除命令结构体
	int  commit_ack_pu32PhyAddr;										// commit_ack 对应的物理地址
	
};


VIM_S32 USB_SAMPLE_COMM_VENC_StartGetStream(VIM_U32 pthread_id,VIM_S32 s32Cnt,VIM_S32 s32Index,SAMPLE_ENC_CFG *pEncCfg);

#endif /* End of #ifndef __SAMPLE_COMMON_USB2VENC_H__ */
