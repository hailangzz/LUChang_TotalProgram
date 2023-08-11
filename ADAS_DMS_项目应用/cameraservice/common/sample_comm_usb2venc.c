#include "sample_comm_usb2venc.h"

#define UNUSED_VARIABLE(x) ((void)(x))

struct _WinUSB1_work_sb WinUSB1_work_sb;

static SAMPLE_VENC_GETSTREAM_PARA_S gs_stPara_auto[pthread_maxnum];
static pthread_t gs_VencGetstreamPid[pthread_maxnum];

extern VIM_S32 SAMPLE_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, char *szFilePostfix);

/*
* 申请内存
*/
int WinUSB_Request_Memory(struct _WinUSB1_work_sb *WinUSB1_work_sb_t)
{
	int i;
	int s32Ret;
	char int_name[ENDOINT_MAX_NUM][50] = {"ep4in_ep1in_int", "ep4in_ep2in_int", "ep4in_ep3in_int"};

	s32Ret = VIM_MPI_SYS_MmzAlloc((VIM_U32 *)&WinUSB1_work_sb_t->ep2_ctrl_pu32PhyAddr,
					(void *)&WinUSB1_work_sb_t->ep2_ctrl, "ep2_ctrl", NULL, 4096);

	if (VIM_SUCCESS != s32Ret) {
		printf("VIM_MPI_SYS_MmzAlloc failed = %#X!\n", s32Ret);
		return VIM_FAILURE;
	}

	s32Ret = VIM_MPI_SYS_MmzAlloc((VIM_U32 *)&WinUSB1_work_sb_t->commit_ack_pu32PhyAddr,
						(void *)&WinUSB1_work_sb_t->commit_ack, "commit_ack", NULL, 4096);
	if (VIM_SUCCESS != s32Ret) {
		printf("VIM_MPI_SYS_MmzAlloc failed = %#X!\n", s32Ret);
		return VIM_FAILURE;
	}

	s32Ret = VIM_MPI_SYS_MmzAlloc((VIM_U32 *)&WinUSB1_work_sb_t->bulk_ep1in_dec_pu32PhyAddr,
					(void *)&WinUSB1_work_sb_t->bulk_ep1in_dec, "bulk_ep1in_dec", NULL, 5120);
	if (VIM_SUCCESS != s32Ret) {
		printf("VIM_MPI_SYS_MmzAlloc failed = %#X!\n", s32Ret);
		return VIM_FAILURE;
	}

	for (i = 0; i < ENDOINT_MAX_NUM; i++) {
		s32Ret = VIM_MPI_SYS_MmzAlloc((VIM_U32 *)&WinUSB1_work_sb_t->ep4in_epxin_pu32PhyAddr[i],
					(void *)&WinUSB1_work_sb_t->ep4in_epxin_interrupt[i], int_name[i], NULL, 4096);
		if (VIM_SUCCESS != s32Ret) {
			printf("VIM_MPI_SYS_MmzAlloc failed = %#X!\n", s32Ret);
			return VIM_FAILURE;
		}
	}

	return VIM_SUCCESS;
}

/*
* 打开WinUSB设备节点，获取句柄
*/
int Init_WinUSBx_Device(int index)
{
	int i;

	if (0 == index) {
		char dev_name_0[ENDOINT_MAX_NUM][50] = \
			{WINUSB_EP1_DEVICE_NAME, WINUSB_EP2_DEVICE_NAME, WINUSB_EP3_DEVICE_NAME};
		WinUSB1_work_sb.fd_mem = open("/dev/mem", O_RDWR|O_SYNC);

		if (WinUSB1_work_sb.fd_mem > 0)
			printf("\"%s\" open success\n", "/dev/mem");

		for (i = 0; i < ENDOINT_MAX_NUM; i++) {
			WinUSB1_work_sb.epxout_fds[i].fd = open(dev_name_0[i], O_RDWR);
			if (WinUSB1_work_sb.epxout_fds[i].fd > 0) {
				WinUSB1_work_sb.epxout_fds[i].events = POLLIN;
				printf("\"%s\" open success\n", dev_name_0[i]);
			} else {
				printf("\"%s\" open failed\n", dev_name_0[i]);
				return -1;
			}
		}
	} else if (1 == index) {
		// ......

	} else {
		return -1;
	}

    return 0;
}

/*
* 读取 WinUSB1 EPxOUT 接收的数据
*/

VIM_VOID *WinUSB1_FORM_EPxOUT(void *arg)
{
    int ret = 0;
	int i;

	UNUSED_VARIABLE(arg);
    do {
        ret = poll(WinUSB1_work_sb.epxout_fds, ENDOINT_MAX_NUM, 5000);
        if (0 == ret) {
            printf("%s Poll time out!!!\n", __func__);
			continue;
        } else if (ret < 0) {
            printf("%s poll exe error\n", __func__);
            break;
        } else {
			/*
            * read_buf[0] 物理地址
            * read_buf[1] 一帧码流长度
            */
			pthread_mutex_lock(&WinUSB1_work_sb.gs_epxout_thread_mutex);

			for (i = 0; i < ENDOINT_MAX_NUM; i++) {
				if (WinUSB1_work_sb.epxout_fds[i].revents & POLLIN) {
					read(WinUSB1_work_sb.epxout_fds[i].fd, WinUSB1_work_sb.epxout_buf[i], 32);

					if (WinUSB1_work_sb.epxout_fds[i].fd == WinUSB1_work_sb.epxout_fds[0].fd) {
						// EP1OUT
					    // printf("*** EP1OUT data address = %#X ***\n", WinUSB1_work_sb.epxout_buf[i][0]);
            			// printf("*** EP1OUT data length = %d ***\n", WinUSB1_work_sb.epxout_buf[i][1]);
						WinUSB1_work_sb.enable_ep1out = 1;
					} else if (WinUSB1_work_sb.epxout_fds[i].fd == WinUSB1_work_sb.epxout_fds[1].fd) {
						// EP2OUT
						printf("****** EP2OUT data address = %#X *****\n", WinUSB1_work_sb.epxout_buf[i][0]);
            			printf("****** EP2OUT data length = %d *****\n", WinUSB1_work_sb.epxout_buf[i][1]);

						WinUSB1_work_sb.ep2_ctrl = (gs_ctrl_order *)mmap(NULL, WinUSB1_work_sb.epxout_buf[i][1],
																				PROT_READ|PROT_WRITE, MAP_SHARED,
																				WinUSB1_work_sb.fd_mem,
																				WinUSB1_work_sb.epxout_buf[i][0]);

						printf("WinUSB1_work_sb.ep2_ctrl->header.dwtype = %#X\n",
							WinUSB1_work_sb.ep2_ctrl->header.dwtype);
						printf("WinUSB1_work_sb.ep2_ctrl->header.payload_type = %#X\n",
							WinUSB1_work_sb.ep2_ctrl->header.payload_type);
						printf("WinUSB1_work_sb.ep2_ctrl->header.len = %d\n",
							WinUSB1_work_sb.ep2_ctrl->header.len);
						printf("WinUSB1_work_sb.ep2_ctrl->header.data_offset = %d\n",
							WinUSB1_work_sb.ep2_ctrl->header.data_offset);
						printf("WinUSB1_work_sb.ep2_ctrl->tlv.tag = %#X\n",
							WinUSB1_work_sb.ep2_ctrl->tlv.tag);
						printf("WinUSB1_work_sb.ep2_ctrl->tlv.len = %d\n",
							WinUSB1_work_sb.ep2_ctrl->tlv.len);

						switch (WinUSB1_work_sb.ep2_ctrl->tlv.tag) {
							case VM_USB_COMMIT:
								printf("############################ VM_USB_COMMIT ############################\n");
								memset(WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN], 0x00, sizeof(gs_interrupt_data));
								WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN]->header.dwtype = 0xFAFAFAFA;
								WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN]->header.ver = 1;
								WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN]->header.payload_type = 0x20;
								WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN]->header.len = 16;
								WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN]->tlv.tag = 0x5011;
								WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN]->tlv.len =
																					sizeof(gs_interrupt_bulkin_data_len);
								WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN]->bulkin_data.bulkin_num = 1;
								WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN]->bulkin_data.data_len =
																					sizeof(gs_ctrl_commit_ack);
								WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP2_IN]->bulkin_data.channel_num = 0;

								memset(WinUSB1_work_sb.commit_ack, 0x00, sizeof(gs_ctrl_commit_ack));
								WinUSB1_work_sb.commit_ack->header.dwtype = 0xFAFAFAFA;
								WinUSB1_work_sb.commit_ack->header.ver = 1;
								WinUSB1_work_sb.commit_ack->header.payload_type =
														WinUSB1_work_sb.ep2_ctrl->header.payload_type;
								WinUSB1_work_sb.commit_ack->header.len = WinUSB1_work_sb.ep2_ctrl->header.len;
								WinUSB1_work_sb.commit_ack->header.data_offset = 0;
								WinUSB1_work_sb.commit_ack->tlv.tag = VM_USB_COMMIT_ACK;
								WinUSB1_work_sb.commit_ack->tlv.len = sizeof(bulk_commit_ack);
								WinUSB1_work_sb.commit_ack->ack.channel_id = 0;
								WinUSB1_work_sb.commit_ack->ack.nstatus = 0;

								memset(&WinUSB1_work_sb.user_data_new[WinUSB_EP2IN], 0x00,
									    sizeof(struct _WinUSB_user_data_new));
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].Data_Package_Size[0] =
														sizeof(gs_ctrl_commit_ack);
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].Data_Package_PhyAddr[0] =
														WinUSB1_work_sb.commit_ack_pu32PhyAddr;
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].Data_Package_Num = 1;
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].Ep_Attribute = INT_AND_BULK;
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].Ep_Select = EP4IN_AND_EP2IN;
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].Interrupt_Data_Size = 1024;
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].Interrupt_Data_PhyAddr =
														WinUSB1_work_sb.ep4in_epxin_pu32PhyAddr[WinUSB_EP4IN_EP2_IN];
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].Standby_Application_PhyAddr = 0;   // Temporary unused
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].data_dispose.enable_time_out = ENABLE_TIMEOUT_ON;
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].data_dispose.data_timeout = 2000;
								WinUSB1_work_sb.user_data_new[WinUSB_EP2IN].data_dispose.channel_num = 0;

								write(WinUSB1_work_sb.epxout_fds[WinUSB_DEV_2].fd,
										&WinUSB1_work_sb.user_data_new[WinUSB_EP2IN], sizeof(struct _WinUSB_user_data_new));

								ioctl(WinUSB1_work_sb.epxout_fds[WinUSB_DEV_2].fd, VM_USB_CTRL_RESETCHN, 0);
								WinUSB1_work_sb.enable_commit = 1;
							break;
							case VM_SEC_CLOSE:
								printf("############################ VM_SEC_CLOSE #######################\n");
								WinUSB1_work_sb.enable_commit = 0;
								WinUSB1_work_sb.enable_ep1out = 0;
								ioctl(WinUSB1_work_sb.epxout_fds[WinUSB_DEV_2].fd, VM_USB_CTRL_RESETCHN, 0);
							break;

							default:
								printf("############################ default ############################\n");
							break;
						}

						munmap(WinUSB1_work_sb.ep2_ctrl, WinUSB1_work_sb.epxout_buf[i][1]);

					} else {
						// EP3OUT
						printf("************ EP3OUT data address = %#X ************\n", WinUSB1_work_sb.epxout_buf[i][0]);
						printf("************ EP3OUT data length = %d ************\n", WinUSB1_work_sb.epxout_buf[i][1]);
					}
				}
			}
			pthread_mutex_unlock(&WinUSB1_work_sb.gs_epxout_thread_mutex);
		}
    } while (!WinUSB1_work_sb.flag_end_winusb);

	SAMPLE_VENC_PRT("%s exit\n", __func__);

	return NULL;
}

/******************************************************************************
* funciton : get stream from each channels,then send them
******************************************************************************/
VIM_VOID *USB_SAMPLE_COMM_VENC_GetVencStreamProc_wait(VIM_VOID *p)
{
	VIM_S32 i = 0;
	VIM_S32 x = 0;
	VIM_S32 s32ChnTotal;
	VENC_CHN_ATTR_S stVencChnAttr;
	SAMPLE_VENC_GETSTREAM_PARA_S *pstPara;
	// VIM_CHAR aszFileName[VENC_MAX_CHN_NUM][128];
	// FILE* pFile[VENC_MAX_CHN_NUM];
	VIM_CHAR szFilePostfix[10];
	VENC_CHN_STAT_S stStat;
	VENC_STREAM_S stStream;
	VIM_S32 s32Ret = VIM_SUCCESS;
	VIM_S32 writeCount;
	VENC_CHN VencChn = 0;
	PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
	VIM_S32 stream_count = 0;
	///<VIM_S32 stream_count_sum = 0;
	VENC_ROI_CFG_S stVencRoiCfg[MAX_COUNT_ROI];
	///<VIM_U32 w = 0;
	///<VIM_U32 h = 0;
	VIM_U32 cur_count = 0;
	SAMPLE_ENC_CFG *pEncCfg;
	VIM_S32 RoiNum = 0;
	///<VIM_S32 cnt_index = 0;
	///<pthread_t gs_VencSendstreamPid;
	///<SAMPLE_VENC_SENDSTREAM_PARA_S stsendstream_s;
	VIM_S32 timeout = 0;

	pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
	s32ChnTotal = pstPara->s32Cnt;
	pEncCfg = (SAMPLE_ENC_CFG *)pstPara->pEncCfg;
	if(NULL == pEncCfg)
		return NULL;

	///<cnt_index = pstPara->s32Index;

	VIM_S8 threadname[16];

	memset(threadname, 0x0, 16);
	if (pEncCfg) {
	  sprintf((char *)threadname, "GetStream%d", pEncCfg->VeChn);
	  prctl(PR_SET_NAME, threadname);
	}
	/******************************************
	 step 1:  check & prepare save-file & venc-fd
	******************************************/
	if (s32ChnTotal >= VENC_MAX_CHN_NUM) {
		SAMPLE_VENC_ERR("input count invaild\n");
		return NULL;
	}

	printf("%s enter s32ChnTotal = %d!\n", __func__, s32ChnTotal);
	for (i = 0; i < s32ChnTotal; i++) {
		/* decide the stream file name, and open file to save stream */
		//VencChn = gs_Vencchn[i];
		VencChn = (pEncCfg+i)->VeChn;
		// 获取视频编码通道属性
		s32Ret = VIM_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
		if (s32Ret != VIM_SUCCESS) {
			SAMPLE_VENC_ERR("VIM_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", VencChn, s32Ret);
			return NULL;
		}
		enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

		s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
		if (s32Ret != VIM_SUCCESS) {
			SAMPLE_VENC_ERR("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n",
					   stVencChnAttr.stVeAttr.enType, s32Ret);
			return NULL;
		}
		SAMPLE_VENC_PRT("enable_nosavefile = 0x%x!\n", (pEncCfg+i)->enable_nosavefile);

		if (0 == (pEncCfg+i)->enable_nosavefile) {
			///<w = stVencChnAttr.stVeAttr.stAttrSvac.u32PicWidth;
			///<h = stVencChnAttr.stVeAttr.stAttrSvac.u32PicHeight;
			// snprintf(aszFileName[i],128, "./stream%d_chn%d_%d_%d_%dbit%s", pstPara->s32Index,
			// 	VencChn,w,h,(pEncCfg+i)->u32PixelMode,szFilePostfix);

			// sprintf((char *)aszFileName[i], "%s_%d%s",(pEncCfg+i)->streamname,cnt_index,szFilePostfix);
			// SAMPLE_VENC_PRT("aszFileName=%s!\n",aszFileName[i]);
			// pFile[i] = fopen(aszFileName[i], "wb");
			// if (!pFile[i]){
			// 	SAMPLE_VENC_ERR("open file[%s] failed!\n",aszFileName[i]);
			// 	return NULL;
			// }

		}
	}

	/******************************************
	 step 2:  Start to get streams of each channel.
	******************************************/
	while (VIM_TRUE == pstPara->bThreadStart) {
		if (WinUSB1_work_sb.enable_commit && WinUSB1_work_sb.enable_ep1out) {
			for (i = 0; i < s32ChnTotal; i++) {
				VencChn = (pEncCfg+i)->VeChn;
				/*******************************************************
				 step 2.1 : query how many packs in one-frame stream.
				*******************************************************/
				memset(&stStream, 0, sizeof(stStream));
				// 查询视频编码通道信息
				s32Ret = VIM_MPI_VENC_Query(VencChn, &stStat);
				if (VIM_SUCCESS != s32Ret) {
					SAMPLE_VENC_ERR("VIM_MPI_VENC_Query chn[%d] failed with %#x!\n", VencChn, s32Ret);
					//pstPara->bThreadStart = VIM_FALSE;
					//break;
				}

				if (0 == stStat.u32CurPacks) {
					SAMPLE_VENC_ERR("NOTE: Current  frame is NULL!\n");
					continue;
				}
				/*******************************************************
				 step 2.3 : malloc corresponding number of pack nodes.
				*******************************************************/
				stStream.pstPack[0] = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
				if (NULL == stStream.pstPack[0]) {
					SAMPLE_VENC_ERR("malloc stream pack failed!\n");
					break;
				}

				memset(stStream.pstPack[0], 0x0, sizeof(VENC_PACK_S) * stStat.u32CurPacks);

				for (cur_count = 1; cur_count < stStat.u32CurPacks; cur_count++) {
					stStream.pstPack[cur_count] = stStream.pstPack[cur_count-1] + 1;
				}

				SAMPLE_COMM_VENC_ParseRoiCfgFile((pEncCfg+i), stVencRoiCfg, stream_count, (VIM_U32 *)&RoiNum);
				for (x = 0; x < RoiNum; x++) {
					// 设置视频编码通道 ROI 属性
					s32Ret = VIM_MPI_VENC_SetRoiCfg(VencChn, &stVencRoiCfg[x]);
					if (s32Ret) {
						SAMPLE_VENC_ERR("VIM_MPI_VENC_SetRoiCfg faild with %#x!\n", s32Ret);
					}
				}
				/******************************************************
				 step 2.4 : call mpi to get one-frame stream
				*******************************************************/
				stStream.u32PackCount = stStat.u32CurPacks;
				/*
					获取视频编码通道码流
				*/

				s32Ret = VIM_MPI_VENC_GetStream(VencChn, &stStream, 3000);
				if (VIM_SUCCESS != s32Ret) {
					free(stStream.pstPack[0]);
					stStream.pstPack[0] = NULL;
					SAMPLE_VENC_ERR("VencChn:%d VIM_MPI_VENC_GetStream failed with %#x!\n", VencChn, s32Ret);
					timeout++;

					if (timeout > 10) {
						//SAMPLE_VENC_ERR("timrout and reboot!\n");
						//system("reboot");
					}
					break;
				}
				/*******************************************************
				 step 2.5 : send frame to pc
				*******************************************************/
				(pEncCfg+i)->enable_nosavefile = 0;//use usb
				if ((pEncCfg+i)->enable_nosavefile == 0) {
					if (VencChn == 0) {
						memset(WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN], 0x00, sizeof(gs_interrupt_data));
						// WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->header.dwtype = stStream.pstPack[0]->u32Len + 5120;
						WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->header.dwtype = 0xFAFAFAFA;
						WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->header.ver = 1;
						WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->header.payload_type = 0x20;
						WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->header.len = 16;
						WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->tlv.tag = 0x5011;
						WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->tlv.len = sizeof(gs_interrupt_bulkin_data_len);
						WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->bulkin_data.bulkin_num = 0;
						WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->bulkin_data.data_len = stStream.pstPack[0]->u32Len + 5120;
						WinUSB1_work_sb.ep4in_epxin_interrupt[WinUSB_EP4IN_EP1_IN]->bulkin_data.channel_num = VencChn;

						memset(WinUSB1_work_sb.bulk_ep1in_dec, 0x00, 5120);
						WinUSB1_work_sb.bulk_ep1in_dec->header.dwtype = 0xFAFAFAFA;
						WinUSB1_work_sb.bulk_ep1in_dec->header.ver = 1;
						WinUSB1_work_sb.bulk_ep1in_dec->header.payload_type = VM_USB_DEC_UNCOMPRESSEDFRAME;
						WinUSB1_work_sb.bulk_ep1in_dec->header.len = 16;

						WinUSB1_work_sb.bulk_ep1in_dec->bulkin_dec.frameType = stStream.reserved[2]; 							// I 帧为 0，P、B帧都为 2
						WinUSB1_work_sb.bulk_ep1in_dec->bulkin_dec.nWidth = stVencChnAttr.stVeAttr.stAttrSvac.u32PicWidth;		// 码流宽度
						WinUSB1_work_sb.bulk_ep1in_dec->bulkin_dec.nHeight = stVencChnAttr.stVeAttr.stAttrSvac.u32PicHeight;	// 码流高度
						WinUSB1_work_sb.bulk_ep1in_dec->bulkin_dec.decryp_data_length = stStream.pstPack[0]->u32Len;			// 码流长度
						WinUSB1_work_sb.bulk_ep1in_dec->bulkin_dec.stream_offset = 5120;										// 码流数据偏移

						memset(&WinUSB1_work_sb.user_data_new[WinUSB_EP1IN], 0x00, sizeof(struct _WinUSB_user_data_new));
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Data_Package_Size[0]    =  5120;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Data_Package_PhyAddr[0] =
							                                          WinUSB1_work_sb.bulk_ep1in_dec_pu32PhyAddr;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Data_Package_Size[1]    =  stStream.pstPack[0]->u32Len;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Data_Package_PhyAddr[1] =
							                                          stStream.pstPack[0]->u32PhyAddr;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Data_Package_Num = 2;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Ep_Attribute = INT_AND_BULK;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Ep_Select = EP4IN_AND_EP1IN;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Interrupt_Data_Size = 1024;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Interrupt_Data_PhyAddr =
							                                WinUSB1_work_sb.ep4in_epxin_pu32PhyAddr[WinUSB_EP4IN_EP1_IN];
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].Standby_Application_PhyAddr = 0;   // Temporary unused
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].data_dispose.enable_time_out = ENABLE_TIMEOUT_ON;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].data_dispose.data_timeout = 2000;
						WinUSB1_work_sb.user_data_new[WinUSB_EP1IN].data_dispose.channel_num = VencChn;

						writeCount = write(WinUSB1_work_sb.epxout_fds[WinUSB_DEV_1].fd,
							          &WinUSB1_work_sb.user_data_new[WinUSB_EP1IN], sizeof(struct _WinUSB_user_data_new));

						if (writeCount == 104 || writeCount == 108 || WinUSB1_work_sb.enable_commit == 0) {
							printf("\n[application layer:] VencChn%d, transfer Frame %d: %d bytes, write %d bytes\n",
																	VencChn,
																	stStream.yuvSeqNum,
																	stStream.pstPack[0]->u32Len,
																	writeCount);
						}
						if (WinUSB1_work_sb.flag_end_winusb) {
							goto GetVencStream_END;
						}
					}

				}
				/*******************************************************
				 step 2.6 : release stream
				*******************************************************/
				s32Ret = VIM_MPI_VENC_ReleaseStream(VencChn, &stStream);
				if (VIM_SUCCESS != s32Ret) {
					SAMPLE_VENC_ERR("Release stream failed!\n");
					free(stStream.pstPack[0]);
					stStream.pstPack[0] = NULL;
					break;
				}
				/*******************************************************
				 step 2.7 : free pack nodes
				*******************************************************/
				free(stStream.pstPack[0]);
				stStream.pstPack[0] = NULL;
			}
		}

	}

GetVencStream_END:
	pEncCfg->dowork_flag = 0;
	SAMPLE_VENC_PRT("%s exit\n", __func__);

	return NULL;
}

VIM_S32 USB_SAMPLE_COMM_VENC_StartGetStream(VIM_U32 pthread_id, VIM_S32 s32Cnt,
                                                          VIM_S32 s32Index, SAMPLE_ENC_CFG *pEncCfg)
{
	VIM_S32 s32Ret = VIM_FAILURE;

	if (pthread_id == 0) {
		if (Init_WinUSBx_Device(0))
			return VIM_FAILURE;

		if (WinUSB_Request_Memory(&WinUSB1_work_sb))
			return VIM_FAILURE;

		pthread_mutex_init(&WinUSB1_work_sb.gs_epxout_thread_mutex, NULL);
	}

	if (pthread_id < pthread_maxnum) {
		gs_stPara_auto[pthread_id].bThreadStart = VIM_TRUE;
		gs_stPara_auto[pthread_id].s32Cnt = s32Cnt;
		gs_stPara_auto[pthread_id].s32Index = s32Index;
		gs_stPara_auto[pthread_id].pEncCfg = pEncCfg;

		if (pEncCfg->test_async_mode == 2) {
			pthread_create(&gs_VencGetstreamPid[pthread_id], 0,
				USB_SAMPLE_COMM_VENC_GetVencStreamProc_wait, (VIM_VOID *)&gs_stPara_auto[pthread_id]);
		} else {
			if (pEncCfg->venc_bind == 0) {
				///< pthread_create(&gs_VencGetstreamPid[pthread_id], 0,
				///<SAMPLE_COMM_VENC_SendFrame_Getstream_Wait, (VIM_VOID*)&gs_stPara_auto[pthread_id]);
			} else if (pEncCfg->venc_bind != 0) {
				pthread_create(&gs_VencGetstreamPid[pthread_id], 0,
					USB_SAMPLE_COMM_VENC_GetVencStreamProc_wait,
					(VIM_VOID *)&gs_stPara_auto[pthread_id]);

				pthread_create(&WinUSB1_work_sb.gs_epxout_Pid, 0,
					WinUSB1_FORM_EPxOUT, NULL);
			}
		}
		return s32Ret;
	}

	return VIM_SUCCESS;
}
