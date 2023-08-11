#include "sample_comm_usb2vdec.h"

int fd_winusb = 0, fd_ep2_winusb = 0, fd_ep3_winusb = 0, fd_mem;
struct pollfd fds[1], fds_ep2[1], fds_ep3[1];
VIM_S32 read_buf[32], read_ep2_buf[32];
testctrl *ep2_ctrl;

long Rec_deltaTime;
struct timeval Rec_tBegin, Rec_tEnd;
long Send_deltaTime;
struct timeval Send_tBegin, Send_tEnd;

struct _WinUSB_user_data_new WinUSB_user_data_new;

struct vimVB_CONF_S usb2vdec_gVbConfig = {0};
testinterrupt *winusb_test_ep4in_ep1in_interrupt;
VIM_U32 usb2vdec_pu32PhyAddr = 0;

/*  [说明] 读取USB发来的命令
    [参数] ...
    [返回值] VIM_SUCCESS 成功，其他失败(目前没做退出)     */
VIM_U32 USB_EP2_CMD(SAMPLE_VDEC_CFG* testParam)
{
    VIM_S32 ret = 0;

    ((void)(testParam));

    do {
        ret = poll(fds_ep2, 1, 1000);
        if (ret == 0) {
            // VDEC_SAMPLE_DEBUG(" Poll usb ep2 device: time out \n");
        } else {
           /*
           * read_ep2_buf 结构
           * read_ep2_buf[0] 物理地址
           * read_ep2_buf[1] CMD长度,有时会携带 I 帧
           */
            read(fd_ep2_winusb, read_ep2_buf, 32);
            // printf("************ read_ep2_buf[0] = %#X ************\n", read_ep2_buf[0]);
            // printf("************ read_ep2_buf[1] = %d ************\n", read_ep2_buf[1]);
            // 目前只映射 CMD 长度

            ep2_ctrl = (testctrl *)mmap(NULL, read_ep2_buf[1], PROT_READ|PROT_WRITE, MAP_SHARED, 
                                                        fd_mem, read_ep2_buf[0]);
            
            printf("ep2_ctrl->header.dwtype = %#X\n", ep2_ctrl->header.dwtype);
            printf("ep2_ctrl->header.ver = %d\n", ep2_ctrl->header.ver);
            printf("ep2_ctrl->header.payload_type = %#X\n", ep2_ctrl->header.payload_type);
            printf("ep2_ctrl->header.len = %d\n", ep2_ctrl->header.len);
            printf("ep2_ctrl->header.data_offset = %d\n", ep2_ctrl->header.data_offset);
            printf("ep2_ctrl->header.res[0] = %d\n", ep2_ctrl->header.res[0]);
            printf("ep2_ctrl->header.res[1] = %d\n", ep2_ctrl->header.res[1]);
            printf("ep2_ctrl->header.res[2] = %d\n", ep2_ctrl->header.res[2]);
            printf("ep2_ctrl->header.res[3] = %d\n", ep2_ctrl->header.res[3]);
            printf("ep2_ctrl->header.tlv.tag = %#X\n", ep2_ctrl->tlv.tag);
            printf("ep2_ctrl->header.tlv.len = %d\n", ep2_ctrl->tlv.len);

            if (USB_CTRL_RESETCHN == ep2_ctrl->tlv.tag) {                         
                ioctl(fd_ep2_winusb, USB_CTRL_RESETCHN, 0);
            }
            munmap(ep2_ctrl, read_ep2_buf[1]);
            
            // ioctl(fd_ep2_winusb, RELEASE_EP2OUT_REQUEST, 0);
            // ioctl(fd_ep2_winusb, RECEIVE_EP2OUT_REQUEST, 0); 
            // ioctl(fd_ep2_winusb, EP2OUT_QUEUE_REQUEST, 0);
        }
    } while (!SAMPLE_COMM_VDEC_GetEndFlag());

    return VIM_SUCCESS;
}

/*  [说明] 读取USB来的码流文件并送入解码器
    [参数] ...
    [返回值] VIM_SUCCESS 成功，其他失败(目前没做退出)     */
VIM_S32 USB_Form_Stream(SAMPLE_VDEC_CFG* testParam)
{
    VIM_S32 ret = 0;
    VDEC_STREAM_S feedStream;

    memset(&feedStream,0,sizeof(VDEC_STREAM_S));

    do {
        ret = poll(fds, 1, 1000);
        if (ret == 0) {      
            VDEC_SAMPLE_DEBUG(" Poll usb ep1 device: time out \n");
        } else {
  
            // gettimeofday(&Rec_tEnd, NULL);
            // Rec_deltaTime = 1000000L * (Rec_tEnd.tv_sec - Rec_tBegin.tv_sec ) + 
            //                                 (Rec_tEnd.tv_usec - Rec_tBegin.tv_usec);
            // printf("      *** Poll Time spent: %ld us\n", Rec_deltaTime);

            /*
            * read_buf 结构
            * read_buf[0] 物理地址
            * read_buf[1] 一帧码流长度 
            */
            read(fd_winusb, read_buf, 32);
       
            feedStream.pu8Addr = mmap(NULL, read_buf[1], PROT_READ|PROT_WRITE, MAP_SHARED, 
                                                        fd_mem, read_buf[0]);

            feedStream.u32Len = read_buf[1];
            feedStream.bEndofStream = VIM_FALSE;
            feedStream.bEndofFrame  = VIM_FALSE;

            // char *frme_name = malloc(100);
            // sprintf(frme_name, "%s%s_%d", "1_chenfq_work/", "frme", frme_number);
            // frme_number++;
            // fd_file_a_frme = open(frme_name, O_WRONLY | O_CREAT, 0777);
            // ret = write(fd_file_a_frme, feedStream.pu8Addr, feedStream.u32Len);
            // if (ret != feedStream.u32Len) {
            //     printf("******************************************************************\n");  
            //     printf("******************************************************************\n");  
            //     printf("******************************************************************\n");  
            //     printf("******************************************************************\n");  
            //     printf("******************************************************************\n");     
            // }
            // close(fd_file_a_frme);
            // munmap(feedStream.pu8Addr, feedStream.u32Len);
            //         ioctl(fd_winusb, RELEASE_EP1OUT_REQUEST, 0);
            //         ioctl(fd_winusb, RECEIVE_EP1OUT_REQUEST, 0);

            do {
                ret = VIM_MPI_VDEC_FeedingStream(testParam->VdecChn, &feedStream, 0);
                if (ret == VIM_SUCCESS) {
                    munmap(feedStream.pu8Addr, feedStream.u32Len);
                    // ioctl(fd_winusb, RELEASE_EP1OUT_REQUEST, 0);
                    // ioctl(fd_winusb, RECEIVE_EP1OUT_REQUEST, 0);   
                    // ioctl(fd_winusb, EP1OUT_QUEUE_REQUEST, 0);

                    // gettimeofday(&Rec_tBegin, NULL);      

                    break;
                } else if (ret == VIM_ERR_VDEC_NOBUF || ret == VIM_ERR_VDEC_TIMEOUT || ret == VIM_ERR_VDEC_CHN_PAUSED) {
                    printf(" VIM_MPI_VDEC_FeedingStream fail ret = %#X\n", ret);
                    usleep(1000);
                    continue;
                }
            } while (!SAMPLE_COMM_VDEC_GetEndFlag()); 

        }
    } while (!SAMPLE_COMM_VDEC_GetEndFlag());

    if (feedStream.pu8Addr != VIM_NULL)
        free (feedStream.pu8Addr);
    VDEC_SAMPLE_DEBUG ("feeding thread return, ret = %d, chn = %d\n", ret, testParam->VdecChn);

    return VIM_SUCCESS;

}

/*  [说明] 初始化USB设备
    [参数] ...
    [返回值] ...        */
VIM_S32 InitUsbDevice( void )
{

    fd_mem = open("/dev/mem", O_RDWR|O_SYNC);
    if (fd_mem > 0) {
        VDEC_SAMPLE_DEBUG("\"%s\" open success\n", "/dev/mem");
    }
#if 1
    fd_winusb = open(WINUSB_EP1_DEVICE_NAME, O_RDWR);
    if (fd_winusb > 0) {
        fds[0].fd     = fd_winusb;
        fds[0].events = POLLIN;
        VDEC_SAMPLE_DEBUG("\"%s\" open success\n", WINUSB_EP1_DEVICE_NAME);
    } else {
        VDEC_SAMPLE_ERR("\"%s\" open failed, fd = %d\n", WINUSB_EP1_DEVICE_NAME, fd_winusb);
        return -2;
    }
    fd_ep2_winusb = open(WINUSB_EP2_DEVICE_NAME, O_RDWR);
    if (fd_ep2_winusb > 0) {
        fds_ep2[0].fd     = fd_ep2_winusb;
        fds_ep2[0].events = POLLIN;
        VDEC_SAMPLE_DEBUG("\"%s\" open success\n", WINUSB_EP2_DEVICE_NAME);    
    } else {
        VDEC_SAMPLE_ERR("\"%s\" open failed, fd = %d\n", WINUSB_EP2_DEVICE_NAME, fd_ep2_winusb);
        return -2;
    }
    fd_ep3_winusb = open(WINUSB_EP3_DEVICE_NAME, O_RDWR);
    if (fd_ep3_winusb > 0) {
        fds_ep3[0].fd     = fd_ep3_winusb;
        fds_ep3[0].events = POLLIN;
        VDEC_SAMPLE_DEBUG("\"%s\" open success\n", WINUSB_EP3_DEVICE_NAME);    
    } else {
        VDEC_SAMPLE_ERR("\"%s\" open failed, fd = %d\n", WINUSB_EP3_DEVICE_NAME, fd_ep3_winusb);
        return -2;
    }
#endif 

#if 0
    fd_winusb = open(WINUSB_EP1_1_DEVICE_NAME, O_RDWR);
    if (fd_winusb > 0) {
        fds[0].fd     = fd_winusb;
        fds[0].events = POLLIN;
        VDEC_SAMPLE_DEBUG("\"%s\" open success\n", WINUSB_EP1_1_DEVICE_NAME);
    } else {
        VDEC_SAMPLE_ERR("\"%s\" open failed, fd = %d\n", WINUSB_EP1_1_DEVICE_NAME, fd_winusb);
        return -2;
    }
    fd_ep2_winusb = open(WINUSB_EP2_1_DEVICE_NAME, O_RDWR);
    if (fd_ep2_winusb > 0) {
        fds_ep2[0].fd     = fd_ep2_winusb;
        fds_ep2[0].events = POLLIN;
        VDEC_SAMPLE_DEBUG("\"%s\" open success\n", WINUSB_EP2_1_DEVICE_NAME);    
    } else {
        VDEC_SAMPLE_ERR("\"%s\" open failed, fd = %d\n", WINUSB_EP2_1_DEVICE_NAME, fd_ep2_winusb);
        return -2;
    }
    fd_ep3_winusb = open(WINUSB_EP3_1_DEVICE_NAME, O_RDWR);
    if (fd_ep3_winusb > 0) {
        fds_ep3[0].fd     = fd_ep3_winusb;
        fds_ep3[0].events = POLLIN;
        VDEC_SAMPLE_DEBUG("\"%s\" open success\n", WINUSB_EP3_1_DEVICE_NAME);    
    } else {
        VDEC_SAMPLE_ERR("\"%s\" open failed, fd = %d\n", WINUSB_EP3_1_DEVICE_NAME, fd_ep3_winusb);
        return -2;
    }
#endif 

    return VIM_SUCCESS;
}

VIM_S32 USB2VDEC_SYS_Init(void)
{
	MPP_SYS_CONF_S stSysConf = {0};
	VIM_S32 ret = VIM_FAILURE;

	VIM_MPI_SYS_Exit();
	VIM_MPI_VB_Exit();

	ret = VIM_MPI_VB_SetConf(&usb2vdec_gVbConfig);
	if( VIM_SUCCESS != ret )
	{
		printf( "VIM_MPI_VB_SetConf failed!\n" );
		return VIM_FAILURE;
	}

	ret = VIM_MPI_VB_Init();
	if( VIM_SUCCESS != ret )
	{
		printf( "VIM_MPI_VB_Init failed!\n" );
		return VIM_FAILURE;
	}

	stSysConf.u32AlignWidth = 32;
	ret = VIM_MPI_SYS_SetConf( &stSysConf );
	if( VIM_SUCCESS != ret )
	{
		printf( "VIM_MPI_SYS_SetConf failed\n" );
		return VIM_FAILURE;
	}

	ret = VIM_MPI_SYS_Init();
	if( VIM_SUCCESS != ret )
	{
		printf( "VIM_MPI_SYS_Init failed!\n" );
		return VIM_FAILURE;
	}

    ret = VIM_MPI_SYS_MmzAlloc(&usb2vdec_pu32PhyAddr, (void*)&winusb_test_ep4in_ep1in_interrupt, "WinUSB_int", NULL, 1024);
	if( VIM_SUCCESS != ret )
	{
		printf( "VIM_MPI_SYS_Init failed = %#X!\n", ret);
		return VIM_FAILURE;
	}

	return VIM_SUCCESS;
}

/*  [说明] 输出结果至USB设备
    [参数] VdecChn 解码通道; sPack 解码输出包
    [返回值] VIM_SUCCESS 成功，其他失败     */

// long Send_deltaTime;
// struct timeval Send_tBegin, Send_tEnd;

VIM_S32 Dump2Usb(VDEC_CHN VdecChn, VDEC_PACK_S *sPack){
    size_t writeCount = 0;
    VIM_S32 ret = VIM_SUCCESS;
    VIM_S32 usb_data_size = 0;

    switch(sPack->sFrame.stVFrame.enPixelFormat){
        case PIXEL_FORMAT_YUV_PLANAR_420:
        case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        default:{

            usb_data_size = sPack->sFrame.stVFrame.u32Size[0] + 
                            sPack->sFrame.stVFrame.u32Size[1] + 
                            sPack->sFrame.stVFrame.u32Size[2];

            // winusb_test_ep4in_ep1in_interrupt->header.dwtype = 0xFAFAFAFA;
			winusb_test_ep4in_ep1in_interrupt->header.dwtype = usb_data_size;
			winusb_test_ep4in_ep1in_interrupt->header.ver = 1;
			winusb_test_ep4in_ep1in_interrupt->header.payload_type = 0x20;
			winusb_test_ep4in_ep1in_interrupt->header.len = 16;
			winusb_test_ep4in_ep1in_interrupt->tlv.tag = 0x5011;
			winusb_test_ep4in_ep1in_interrupt->tlv.len = 8;
			winusb_test_ep4in_ep1in_interrupt->bulkin_data.bulkin_num = 0;			
            winusb_test_ep4in_ep1in_interrupt->bulkin_data.data_len = usb_data_size;
            winusb_test_ep4in_ep1in_interrupt->bulkin_data.channel_num = 0;

            WinUSB_user_data_new.Data_Package_Size[0]    =  usb_data_size;
            WinUSB_user_data_new.Data_Package_PhyAddr[0] =  sPack->sFrame.stVFrame.u32PhyAddr[0];
            WinUSB_user_data_new.Data_Package_Num = 1;
            WinUSB_user_data_new.Ep_Attribute = INT_AND_BULK;
            WinUSB_user_data_new.Ep_Select = EP4IN_AND_EP1IN;
            WinUSB_user_data_new.Interrupt_Data_Size = 1024;
            WinUSB_user_data_new.Interrupt_Data_PhyAddr = usb2vdec_pu32PhyAddr;
            WinUSB_user_data_new.Standby_Application_PhyAddr = 0;   // Temporary unused
            WinUSB_user_data_new.data_dispose.enable_time_out = ENABLE_TIMEOUT_ON;
            WinUSB_user_data_new.data_dispose.data_timeout = 500;
            WinUSB_user_data_new.data_dispose.channel_num = 0;

            while(!SAMPLE_COMM_VDEC_GetEndFlag()){
                
                gettimeofday(&Send_tBegin, NULL);

                writeCount = write(fd_winusb, &WinUSB_user_data_new, sizeof(struct _WinUSB_user_data_new));
                

                gettimeofday(&Send_tEnd, NULL);
                Send_deltaTime = 1000000L * (Send_tEnd.tv_sec - Send_tBegin.tv_sec ) + 
                                                    (Send_tEnd.tv_usec - Send_tBegin.tv_usec);
                if (sPack->sFrame.stVFrame.u32TimeRef % 5000 == 0) {
                    printf("\n[application layer:] chn%d, transfer Frame %d: %d bytes, write %d bytes\n", 
                                                            VdecChn, 
                                                            sPack->sFrame.stVFrame.u32TimeRef, 
                                                            usb_data_size, 
                                                            writeCount);
                    printf("[application layer:] send Time spent: %ld us\n", Send_deltaTime);
                } 

                break;
            }
        }
    }
    if(writeCount == 0)
        ret = VIM_FAILURE;
    return ret;
}
