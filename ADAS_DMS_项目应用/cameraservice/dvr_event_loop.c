/******************************************************************
* Copyright (C), 2001-2022 Beijing Vimicro Technology Co., Ltd.
* ALL Rights Reserved
*
* Name      : main.c
* Summary   :
* Date      : 2022.4.27
* Author    :
*
******************************************************************/

#include <stdio.h>      /**stderr**/
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>      /**errno**/
#include <string.h>     /**memset**/
#include <netinet/in.h> /**INADDR_ANY, IPPROTO_TCP**/
#include <pthread.h>    /**PTHREAD_CREATE_DETACHED**/
#include <sys/time.h>
#include <sys/resource.h>

#include "lc_glob_type.h"
#include "lc_dvr_vpss_api.h"
#include "lc_jpeg_api.h"
#include "lc_dvr_record_log.h"
#include "do_dcvi.h"
#include "vim_comm_vo.h"
#include "vim_comm_video.h"
#include "VimRecordSdk.h"
#include "lc_dvr_socket.h"
#include "lc_dvr_record_sys_func.h"
#include "lc_dvr_RecordCameraExt.h"
#include "lc_dvr_record_mcu_parse.h"

#include "rrtype.h"
#include "lc_dvr_semaphore.h"
#include "lc_dvr_interface_4_vim.h"
#include "lc_dvr_glob_def.h"
#include "duvonn_storage.h"
#include "lc_codec_api.h"
#include "lc_dvr_mem_write_cache.h"
#include "lc_dvr_build_emerge_file.h"
#include "lc_dvr_build_sound_file.h"

#define JPG_TEST_DIR  "/mnt/usb/usb1"


int  g_socket_fd = 0;
int  g_play_master_fd = -1;
static pthread_mutex_t mFileNameMutex;
static pthread_mutex_t mGenNameMutex;
static int mMdvrRunState;
static int mFunTestCnt =1;

extern void mdvr_thread_handle(void *arg);
extern void vc_record_start_again(void);
extern VIM_VOID vc_jpeg_take_picture(int chanel, char* fullpath);

int vim_record_filePath(int ch, int event, char* fullpath)
{	
	//	struct timeval start;
	//	int interval;
	//	gettimeofday(&start, NULL);
	//	interval = 1000000*(start.tv_sec) + (start.tv_usec);
	//	interval /= 1000;
	//	logd_lc("T:[%d]vim_record_filePath(%d)-->%d",interval, event, ch);
	
	logd_lc("vim_record_filePath(%d)-->%d", event, ch);
	pthread_mutex_lock(&mGenNameMutex);

	if(event == 5)
	{
		RecordGenEventfile(fullpath, ch);
	}
	else
	{
		if(is_duvonn_device())
		{			
			switch(ch)
			{
				case LC_DUVONN_CHANEL_0:
				case LC_DUVONN_CHANEL_1:
					{
						int index = duvonn_camera_creat_new_file(ch);
						int len = RecordGetfileDir(fullpath, ch);
						sprintf(fullpath+len, "/%s", duvonn_video_name_get(ch, index));
						RecordInserDuvonnFileName(fullpath, ch, index);
					}					
					break;
				default:
					RecordGenfilename(fullpath, ch);
					break;
			}
		}
		else
		{
			RecordGenfilename(fullpath, ch);
		}
		vc_jpeg_take_picture(ch, fullpath);
	}
	pthread_mutex_unlock(&mGenNameMutex);
	return 0;
}

VIM_S32 vim_mdvr_run(char* recordPath)
{
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if (0 != pthread_create(&tid, &attr, mdvr_thread_handle, NULL))
	{
		loge_lc("create socket thread failed.\n");
		pthread_attr_destroy(&attr);
	}	  
	pthread_attr_destroy(&attr);	
}

void dvr_event_loop(void)
{
#ifdef LC_MDVR_FUNCTION_TEST
	logd_lc("Enable LC_MDVR_FUNCTION_TEST.\n");
#endif
	{
		pthread_t tid;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		if (0 != pthread_create(&tid, &attr, lc_sock_accept_thread, NULL))
		{
			loge_lc("create socket thread failed.\n");
			pthread_attr_destroy(&attr);
		}
		
		pthread_attr_destroy(&attr);
		
		lc_dvr_mcu_connect_initial();
		lc_dvr_mem_write_cache_initial();
	}

	{
		LC_DVR_RECORD_MESSAGE* message;	
		sys_msg_queu_pool_initial();
		mMdvrRunState = 0;		
#ifdef LC_MDVR_FUNCTION_TEST
		if(1)
		{
			pthread_t client_thread[4];
			int ret;
			
		  	usleep(100*1000); /**等待*lc_sock_accept_thread启动*/
			ret = pthread_create(&client_thread[0], NULL, (void*)lc_socket_client_thread, NULL);
			if (0 == ret)
			{
				logd_lc("socket_client0 SUCCESS\n");
				pthread_detach(client_thread[0]);
			}
			sleep(1);
		}
#endif	
		{
			char *vPath = sys_msg_get_memcache(128);
			sprintf(vPath, "%s", "/mnt/nfs/record");
			pthread_mutex_init(&mGenNameMutex, NULL);
			vc_set_mdvr_record_status(0);
			vc_message_send(ENU_LC_DVR_MDVR_RUN, 0, NULL, vPath);
			//-----------------------------------------------------------------				
			/**强制启动录像，用于测试**/
//			char *strPtr = sys_msg_get_memcache(256);
//			memset(strPtr, 0, 256);
//			strcpy(strPtr, LC_DVR_RECORD_BASE_DIR);
//			//strcpy(strPtr+64, "粤B84936"); /**exFat格式SD卡，mount必须加参数，否则中文不支持**/
//			strcpy(strPtr+64, "QB84936");
//			vc_message_send(ENU_LC_DVR_SET_VIDEO_DIR, 0, strPtr, NULL);
//			vc_message_send(ENU_LC_DVR_RECORD_START, 0, NULL, NULL);
			//-----------------------------------------------------------------					
		}
		logd_lc("mdvr begin run......");		
		while(1)
		{
			message = sys_msg_queu_out();
			if(message == NULL) continue;
			logd_lc("message->event-->%d", message->event);
			switch(message->event)
			{
				case ENU_LC_DVR_MOUNT_EVENT: /**SD卡插入或者拔出事件**/
					{
						logd_lc("ENU_LC_DVR_MOUNT_EVENT\n");
						switch(message->value)
						{
							case LC_MOUNT_EVENT:
								logd_lc("mount event from (%s)!\n", message->msg_buf);
								{
									int fd = MSG_SRCID(message->msg_id);
									lc_socket_get_video_dir(fd);
								}
								break;
							case LC_UNMOUNT_EVENT:
								logd_lc("xxxxx ummount event xxxxx!\n");
								// vc_message_send(ENU_LC_DVR_RECORD_STOP, 0, NULL, NULL);
								break;
						}	
					}
					break;
				case ENU_LC_DVR_STREAM_OPEN: /**推流通道启动**/
					{
						LC_VENC_STREAM_INFO* streamInfo = (LC_VENC_STREAM_INFO*)message->msg_ptr;
						int pipeChanel = message->value;
						logd_lc("pipeChanel-->%d", pipeChanel);
						if((pipeChanel>=0)&&(pipeChanel<4))
						{
							int fd = MSG_SRCID(message->msg_id);
							lc_socket_app_fd_set(LC_DVR_APACHE_SERID, fd);						
							logd_lc("ENU_LC_DVR_STREAM_OPEN[%d]-->r:%d, c:%d", pipeChanel, streamInfo->resolution, streamInfo->camChnl);
							/**=================推流用通道==================**/
							vc_stream_venc_config(streamInfo->resolution, pipeChanel);
							vc_stream_out_start(streamInfo->camChnl, 1, pipeChanel);
						}
						
#ifdef LC_MDVR_FUNCTION_TEST
						if(pipeChanel == 1)
						{
							pthread_t streamOut_thread;	/**测试用，通过pipe读视频数据**/
							VIM_S32 ret = pthread_create(&streamOut_thread, NULL, (void*)vc_stream_handler, &pipeChanel);
							if (0 == ret)
							{
								logd_lc("stream pthread SUCCESS\n");
								pthread_detach(streamOut_thread);
							}
						}
#endif
					}
					break;
				case ENU_LC_DVR_STREAM_CLOSE:/**关闭推流通道,切换不同分辨率要先CLOSE然后用需要的参数OPEN**/
					{
						int pipeChanel = message->value;
						logd_lc("ENU_LC_DVR_STREAM_CLOSE");
						vc_stream_out_stop(pipeChanel);
						vc_stream_venc_destroy(pipeChanel);
					}
					break;
				case ENU_LC_DVR_STREAM_CHANEL: //
					{
						int pipeChanel = (message->value>>8)&0xff;
						int camId = (message->value)&0xff;
						logd_lc("ENU_LC_DVR_STREAM_CHANEL -->%d", message->value);
						vc_stream_out_chnlSwitch(camId, pipeChanel);
					}
					break;
				case ENU_LC_DVR_MDVR_RUN: /**录像视频系统运行**/
					if(mMdvrRunState == 0)
					{
						logd_lc("DCR_MDVR_RUN: %s\n", (char*)message->msg_ptr);
						vc_record_para_initial();
						vim_mdvr_run((char*)message->msg_ptr);						
						vc_jpeg_enc_initial(LC_TAKE_STREAM_PIC_SIZE_W,LC_TAKE_STREAM_PIC_SIZE_H,LC_STREAM_ENCODE_CHANEL,4);	/**960x544**/						
						vc_jpeg_enc_initial(LC_TAKE_LARGE_PIC_SIZE_W, LC_TAKE_LARGE_PIC_SIZE_H, LC_LARGE_ENCODE_CHANEL, 5);	/**1280x736**/
						vc_jpeg_enc_initial(LC_TAKE_SMALL_PIC_SIZE_W, LC_TAKE_SMALL_PIC_SIZE_H, LC_SMALL_ENCODE_CHANEL, 5); /**1920x1088**/
						vc_jpeg_enc_initial(LC_TAKE_THUMB_PIC_SIZE_W, LC_TAKE_THUMB_PIC_SIZE_H, LC_THUMB_ENCODE_CHANEL, 3);	/**640x384**/
						mMdvrRunState = 1;
					}
					break;
				case ENU_LC_DVR_MDVR_STOP:
					if(mMdvrRunState!=0)
					{
						mMdvrRunState = 0;
						vc_jpeg_enc_uninitial(LC_STREAM_ENCODE_CHANEL);	
						vc_jpeg_enc_uninitial(LC_LARGE_ENCODE_CHANEL);
						vc_jpeg_enc_uninitial(LC_SMALL_ENCODE_CHANEL);						
						vc_jpeg_enc_uninitial(LC_THUMB_ENCODE_CHANEL);
					}
					break;
				
				case ENU_LC_DVR_MDVR_TAKEPIC: /**拍照线程创建和启动**/
					{
						LC_TAKE_PIC_INFO *picInfo = (LC_TAKE_PIC_INFO*)message->msg_ptr;
						logd_lc("ENU_LC_DVR_MDVR_TAKEPIC: %s", picInfo->pic_save_path);
						/**同时多路拍照会有冲突，所以设置队列来缓存**/
						if(lc_jpeg_get_from_queue()==NULL) 
						{
							lc_jpeg_insert_to_queue(picInfo);
							vc_message_send(ENU_LC_DVR_MDVR_TAKEPIC_START, 0, NULL, NULL);
						}
						else
						{
							lc_jpeg_insert_to_queue(picInfo);
						}
					}
					break;
				case ENU_LC_DVR_MDVR_TAKEPIC_START:	
					{/**拍照处理**/
						LC_TAKE_PIC_INFO *picInfo = (LC_TAKE_PIC_INFO*)lc_jpeg_get_from_queue();
						logd_lc("ENU_LC_DCR_MDVR_TAKEPIC-->%d, %d !!", picInfo->pic_width, picInfo->pic_height);	
						if(picInfo!=NULL)
						{
							JPEG_PIC_CTRL_H *jpgCtr;
							int jpg_chanel;
							if((picInfo->pic_width==LC_TAKE_SMALL_PIC_SIZE_W)&&(picInfo->pic_height==LC_TAKE_SMALL_PIC_SIZE_H))
							{
								jpg_chanel = LC_SMALL_ENCODE_CHANEL;
							}
							else if((picInfo->pic_width==LC_TAKE_LARGE_PIC_SIZE_W)&&(picInfo->pic_height==LC_TAKE_LARGE_PIC_SIZE_H))
							{
								jpg_chanel = LC_LARGE_ENCODE_CHANEL;
							}
							else if((picInfo->pic_width==LC_TAKE_STREAM_PIC_SIZE_W)&&(picInfo->pic_height==LC_TAKE_STREAM_PIC_SIZE_H))
							{
								jpg_chanel = LC_STREAM_ENCODE_CHANEL;
							}
							else if((picInfo->pic_width==LC_TAKE_THUMB_PIC_SIZE_W)&&(picInfo->pic_height==LC_TAKE_THUMB_PIC_SIZE_H))
							{
								jpg_chanel = LC_THUMB_ENCODE_CHANEL;
							}
					
							jpgCtr = lc_jpeg_handler(jpg_chanel);
							if(jpgCtr->u32UsedSign)
							{
								lc_jpeg_do_takePic(picInfo->pic_save_path, picInfo->camChnl, jpg_chanel, picInfo->ackFun);								
								jpgCtr->frameDelay = picInfo->frameDelay;
							}							
						}
					}
					break;
				case ENU_LC_DVR_MDVR_PIC_NOTIFY:
					{						
						//printf("ENU_LC_DVR_MDVR_PIC_NOTIFY-->\n"); /**拍照完成的反馈消息**/
					}
					break;
				case ENU_LC_DVR_MDVR_TAKEPIC_QUIT:
					{

					}
					break;
				case ENU_LC_DVR_MEDIA_PLAY_START:					
					{					
						g_play_master_fd = MSG_SRCID(message->msg_id);
						vc_start_mediaFile_play(message->msg_ptr);	/**启动视频播放**/						
					}
					break;
				case ENU_LC_DVR_MEDIA_PLAY_SEEK:
					{
						double *time = (double*)message->msg_ptr; 
						vc_media_play_seek(time[0]); /**double型变量指定播放的时间点**/
					}
					break;		
				case ENU_LC_DVR_MEDIA_PLAY_PAUSE:
					{
						vc_media_play_pause(message->value);/**1 暂停 0 继续播放**/
					}
					break;	
				case ENU_LC_DVR_MEDIA_PLAY_FAST:
					{
						vc_media_play_speed(message->value); /**1 倍数播放 0 取消倍数播放**/
					}
					break;				
				case ENU_LC_DVR_MEDIA_PLAY_STOP:
					{
						logd_lc("ENU_LC_DVR_MEDIA_PLAY_STOP!!");
						vc_start_mediaFile_stop(); /**退出视频播放**/
						g_play_master_fd = -1;
					}
					break;
				case ENU_LC_DRV_MEDIA_PLAY_REPORT:
					{						
						int infNum;						
						char *mediaPlayInfo = (char*)vc_media_play_notify_info(&infNum);
						LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(sizeof(LC_DVR_RECORD_MESSAGE)+infNum);

						logd_lc("ENU_LC_DRV_MEDIA_PLAY_REPORT!!");
						memcpy(msgAck->msg_buf, mediaPlayInfo, infNum);
						msgAck->event = ENU_LC_DRV_MEDIA_PLAY_REPORT;
						msgAck->value = 0;	
						if(g_play_master_fd>=0)
						{
							lc_socket_client_send(g_play_master_fd, msgAck, sizeof(LC_DVR_RECORD_MESSAGE)+infNum);
						}
					}
					break;
				case ENU_LC_DVR_SET_VIDEO_DIR: /**数据管理进程回送的视频文件目录**/
					
					{
						char *videoDirName = (char*)message->msg_ptr;
						int fd = MSG_SRCID(message->msg_id);
						char fullpath[128];
						
						lc_socket_app_fd_set(LC_DVR_RECO_SERID, fd);
						if(RecordSetVideoPath(videoDirName))
						{
							logd_lc("MDVR SET VIDEO DIR-->%s!!", videoDirName); /**默认开始录像**/
							usleep(500*1000);										
							RecordCameraInitial();	/**初始化视频文件列表**/
							
							for(int camid=0; camid<LC_DVR_SUPPORT_CAMERA_NUMBER; camid++)
							{
								RecordVideoEncParmInit(camid);
								RecordInitFileListDir(camid);
							}
							lc_dvr_sound_record_initial();
							RecordInitEmergencyDir();
							RecordInitSoundRecordDir();
							RecordInitImageRecordDir();
							if(is_duvonn_device())
							{
	//							duvonn_camera_reset_file(LC_DUVONN_CHANEL_0);	/**重新初始化灾备的视频**/
	//							duvonn_camera_reset_file(LC_DUVONN_CHANEL_1);
								
								RecordInitListFromDuvonn(LC_DUVONN_CHANEL_0);
								RecordInitListFromDuvonn(LC_DUVONN_CHANEL_1);
							}
							Record_fileInfo_initial();
							lc_dvr_emergency_initial(); /**报警视频缓存**/
						}
					}
					break;				
				case ENU_LC_DVR_RECORD_START: /**视频录像开始前通过ENU_LC_DVR_SET_VIDEO_DIR设置目录路径**/
					{
						logd_lc("ENU_LC_DVR_RECORD_START");		
						vc_chn_record_start();
						vc_set_mdvr_record_status(1);
					}
					break;
				case ENU_LC_DVR_RECORD_STOP: /**视频录像结束**/
					{
						logd_lc("ENU_LC_DVR_RECORD_STOP");
						lc_dvr_sound_record_stop();
						lc_dvr_emergency_close_all();						
						vc_chn_record_stop();
						vc_set_mdvr_record_status(0);
					}
					break;
				case ENU_LC_DVR_GET_RECORD_STATUS:
					{
						int fd = MSG_SRCID(message->msg_id);
						LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(64);
						memset(msgAck, 0, sizeof(LC_DVR_RECORD_MESSAGE));
						msgAck->event = ENU_LC_DVR_RECORD_STATUS_ACK;
						msgAck->value = vc_get_mdvr_record_status();
						lc_socket_client_send(fd, msgAck, sizeof(LC_DVR_RECORD_MESSAGE));
					}
					break;
				case ENU_LC_DVR_VIDEO_RECORD_RESTART:
					{
						logd_lc("ENU_LC_DVR_VIDEO_RECORD_RESTART");
						vc_record_start_again();
					}
					break;
				case ENU_LC_DVR_DISPLAY_CHANEL:
//					{
//						printf("ENU_LC_DVR_DISPLAY_CHANEL-->%d\n", message->value);
//						vc_vo_display_chanel(message->value);
//					}
					break;
				case ENU_LC_DVR_FETCH_RECORD_LIST:
					if(Is_InfoList_Initial())
					{
						LC_RECORD_LIST_FETCH *fileFetch = (LC_RECORD_LIST_FETCH*)message->msg_buf;							
						LC_RECORD_LIST_FETCH *fileAck; 
						LC_DVR_RECORD_MESSAGE *msgAck;
						
						int length = 16+sizeof(LC_RECORD_LIST_FETCH)+(sizeof(GBT19056_FILE_FETCH_DAT)*fileFetch->fileNumber);
						int fd = MSG_SRCID(message->msg_id);
						int realNum;

						logd_lc("ENU_LC_DVR_FETCH_RECORD_LIST");
						logd_lc("Query-->camChnl:%d, offset:%d, fileNumber:%d", fileFetch->camChnl, fileFetch->offset, fileFetch->fileNumber);
						
						msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(length);
						msgAck->event = ENU_LC_DVR_FETCH_RECORD_LIST_ACK;
						msgAck->value = 0;
						
						fileAck = (LC_RECORD_LIST_FETCH*)msgAck->msg_buf; 
						fileAck->camChnl = fileFetch->camChnl;
						fileAck->offset = fileFetch->offset;    /**起始序号**/
						fileAck->fileNumber = fileFetch->fileNumber; /**需要获取的数量**/
						realNum = RecordFileListFetch(fileAck);
						length = (uint32_t)(fileAck->fileList) - (uint32_t)msgAck+16+realNum*fileAck->bytePerFile;
						lc_socket_client_send(fd, msgAck, length);
					}
					break;
				case ENU_LC_DVR_FETCH_CUR_RECORD:
					if(Is_InfoList_Initial())
					{						
						LC_RECORD_LIST_FETCH *fileAck; 
						LC_DVR_RECORD_MESSAGE *msgAck;						
						int fd = MSG_SRCID(message->msg_id);
						
						logd_lc("ENU_LC_DVR_FETCH_CUR_RECORD");
						
						msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(256);
						msgAck->event = ENU_LC_DVR_FETCH_CUR_RECORD_ACK;
						msgAck->value = 0;
							
						fileAck = (LC_RECORD_LIST_FETCH*)msgAck->msg_buf; 
						fileAck->camChnl = message->value;
						logd_lc("fileAck->camChnl->%d", fileAck->camChnl);
						if((fileAck->camChnl>=0)&&(fileAck->camChnl<4))
						{
							fileAck->offset = -1;
							fileAck->fileNumber = 1;							
							RecordFileListFetch(fileAck);
							lc_socket_client_send(fd, msgAck, sizeof(LC_RECORD_LIST_FETCH)+16);
						}
						logd_lc("cur record finish!");
					}
					break;
				
				case ENU_LC_DVR_FETCH_EMERGNECY_LIST:
					if(Is_InfoList_Initial())
					{
						LC_RECORD_LIST_FETCH *fileFetch = (LC_RECORD_LIST_FETCH*)message->msg_buf;							
						LC_RECORD_LIST_FETCH *fileAck; 
						LC_DVR_RECORD_MESSAGE *msgAck;
						
						int length = 16+sizeof(LC_RECORD_LIST_FETCH)+(sizeof(GBT19056_FILE_FETCH_DAT)*fileFetch->fileNumber);
						int fd = MSG_SRCID(message->msg_id);
						int realNum;

						logd_lc("ENU_LC_DVR_FETCH_EMERGNECY_LIST");
						logd_lc("Query-->camChnl:%d, offset:%d, fileNumber:%d", fileFetch->camChnl, fileFetch->offset, fileFetch->fileNumber);
						
						msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(length);
						msgAck->event = ENU_LC_DVR_FETCH_RECORD_LIST_ACK;
						msgAck->value = 0;
						
						fileAck = (LC_RECORD_LIST_FETCH*)msgAck->msg_buf; 
						fileAck->fileType = fileFetch->fileType;
						fileAck->camChnl = fileFetch->camChnl;
						fileAck->offset = fileFetch->offset;    /**起始序号**/
						fileAck->fileNumber = fileFetch->fileNumber; /**需要获取的数量**/
						realNum = RecordEmergencyListFetch(fileAck);
						length = (uint32_t)(fileAck->fileList) - (uint32_t)msgAck+16+realNum*fileAck->bytePerFile;
						lc_socket_client_send(fd, msgAck, length);
	{
//		GBT19056_FILE_FETCH_DAT* fileList = (GBT19056_FILE_FETCH_DAT*)fileAck->fileList;
//		char lockPath[128];
//		logd_lc("fileList Getxx->%x %s", fileList, fileList->fileName);
//		Record_File_Lock(fileList->fileName, lockPath);
	//	Record_File_unLock(fileList->fileName);
	}						
					}					
					break;
					
				case ENU_LC_DVR_FETCH_CUR_EMERGENCY:
					if(Is_InfoList_Initial())
					{						
						LC_RECORD_LIST_FETCH *fileAck; 
						LC_DVR_RECORD_MESSAGE *msgAck;						
						int fd = MSG_SRCID(message->msg_id);
						
						logd_lc("ENU_LC_DVR_FETCH_CUR_EMERGENCY->%x", message->value);
						
						msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(256);
						msgAck->event = ENU_LC_DVR_FETCH_CUR_RECORD_ACK;
						msgAck->value = 0;
							
						fileAck = (LC_RECORD_LIST_FETCH*)msgAck->msg_buf; 
						fileAck->fileType = (message->value>>8)&0xff;
						fileAck->camChnl = (message->value)&0xff;
						fileAck->offset = -1;
						fileAck->fileNumber = 1;							
						RecordEmergencyListFetch(fileAck);
						
						lc_socket_client_send(fd, msgAck, sizeof(LC_RECORD_LIST_FETCH)+16);
						logd_lc("cur record finish!");
					}					
					break;
				case ENU_LC_DVR_FETCH_AUDIO_LIST:
					if(Is_InfoList_Initial())
					{
						LC_RECORD_LIST_FETCH *fileFetch = (LC_RECORD_LIST_FETCH*)message->msg_buf;							
						LC_RECORD_LIST_FETCH *fileAck; 
						LC_DVR_RECORD_MESSAGE *msgAck;
						
						int length = 16+sizeof(LC_RECORD_LIST_FETCH)+(sizeof(GBT19056_FILE_FETCH_DAT)*fileFetch->fileNumber);
						int fd = MSG_SRCID(message->msg_id);
						int realNum;

						logd_lc("ENU_LC_DVR_FETCH_AUDIO_LIST");
						logd_lc("Query-->camChnl:%d, offset:%d, fileNumber:%d", fileFetch->camChnl, fileFetch->offset, fileFetch->fileNumber);
						
						msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(length);
						msgAck->event = ENU_LC_DVR_FETCH_RECORD_LIST_ACK;
						msgAck->value = 0;
						
						fileAck = (LC_RECORD_LIST_FETCH*)msgAck->msg_buf;
						fileAck->camChnl = fileFetch->camChnl;
						fileAck->offset = fileFetch->offset;    /**起始序号**/
						fileAck->fileNumber = fileFetch->fileNumber; /**需要获取的数量**/
						realNum = RecordAudioListFetch(fileAck);
						length = (uint32_t)(fileAck->fileList) - (uint32_t)msgAck+16+realNum*fileAck->bytePerFile;						
						lc_socket_client_send(fd, msgAck, length);
					}					
					break;
					
				case ENU_LC_DVR_FETCH_CUR_AUDIO:
					if(Is_InfoList_Initial())
					{						
						LC_RECORD_LIST_FETCH *fileAck; 
						LC_DVR_RECORD_MESSAGE *msgAck;						
						int fd = MSG_SRCID(message->msg_id);
						
						logd_lc("ENU_LC_DVR_FETCH_CUR_AUDIO");
						
						msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(256);
						msgAck->event = ENU_LC_DVR_FETCH_CUR_RECORD_ACK;
						msgAck->value = 0;
							
						fileAck = (LC_RECORD_LIST_FETCH*)msgAck->msg_buf; 
						fileAck->camChnl = message->value;
						logd_lc("fileAck->camChnl->%d", fileAck->camChnl);
						fileAck->offset = -1;
						fileAck->fileNumber = 1;							
						RecordAudioListFetch(fileAck);
						lc_socket_client_send(fd, msgAck, sizeof(LC_RECORD_LIST_FETCH)+16);
						logd_lc("cur record finish!");
					}					
					break;		
				case ENU_LC_DVR_FETCH_LOOP_IMAGE_LIST:
					if(Is_InfoList_Initial())
					{
						LC_RECORD_LIST_FETCH *fileFetch = (LC_RECORD_LIST_FETCH*)message->msg_buf;							
						LC_RECORD_LIST_FETCH *fileAck; 
						LC_DVR_RECORD_MESSAGE *msgAck;
						
						int length = 16+sizeof(LC_RECORD_LIST_FETCH)+(sizeof(GBT19056_FILE_FETCH_DAT)*fileFetch->fileNumber);
						int fd = MSG_SRCID(message->msg_id);
						int realNum;

						logd_lc("ENU_LC_DVR_FETCH_IMAGE_LIST");
						logd_lc("Query-->camChnl:%d, offset:%d, fileNumber:%d", fileFetch->camChnl, fileFetch->offset, fileFetch->fileNumber);
						
						msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(length);
						msgAck->event = ENU_LC_DVR_FETCH_RECORD_LIST_ACK;
						msgAck->value = 0;
						
						fileAck = (LC_RECORD_LIST_FETCH*)msgAck->msg_buf;
						fileAck->camChnl = fileFetch->camChnl;
						fileAck->offset = fileFetch->offset;    /**起始序号**/
						fileAck->fileNumber = fileFetch->fileNumber; /**需要获取的数量**/
						realNum = RecordImageListFetch(fileAck);
						length = (uint32_t)(fileAck->fileList) - (uint32_t)msgAck+16+realNum*fileAck->bytePerFile;
						lc_socket_client_send(fd, msgAck, length);
					}					
					break;				
				case ENU_LC_DVR_FETCH_CUR_LOOP_IMAGE:
					if(Is_InfoList_Initial())
					{						
						LC_RECORD_LIST_FETCH *fileAck; 
						LC_DVR_RECORD_MESSAGE *msgAck;						
						int fd = MSG_SRCID(message->msg_id);
						
						logd_lc("ENU_LC_DVR_FETCH_CUR_IMAGE");
						
						msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(256);
						msgAck->event = ENU_LC_DVR_FETCH_CUR_RECORD_ACK;
						msgAck->value = 0;
							
						fileAck = (LC_RECORD_LIST_FETCH*)msgAck->msg_buf; 
						fileAck->camChnl = message->value;
						logd_lc("fileAck->camChnl->%d", fileAck->camChnl);
						fileAck->offset = -1;
						fileAck->fileNumber = 1;							
						RecordImageListFetch(fileAck);
						lc_socket_client_send(fd, msgAck, sizeof(LC_RECORD_LIST_FETCH)+16);
						logd_lc("cur record finish!");
					}						
					break;
				case ENU_LC_DVR_NOTIFY_RECORD_STATUS:
					{
						uint32_t data = message->value;
						logd_lc("SETTINGS_PROP_VIDEO_REC_STATE-->%d", data);
						lc_dvr_mcu_notify(data);
						if(data == 0)
						{							
							while(lc_dvr_mem_cam_busy_status())
							{
								usleep(25*1000);
							}
							Record_fileInfo_dbsave();
							logd_lc("File sync!");
							sync();
							logd_lc("sync finish!");
						}
						logd_lc("set rec state finish!");
					}
					break;
				case ENU_LC_DVR_GB19056_FILE_COPY:
					{
						int fd = MSG_SRCID(message->msg_id);
						lc_socket_app_fd_set(LC_DVR_GUI_SERID, fd);
						GBT19056_COPY_INFO *copyInfo = (GBT19056_COPY_INFO*)message->msg_ptr;
						LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(512);
						
						logd_lc("ENU_LC_DVR_GB19056_FILE_COPY Mdvr route!");						
						logd_lc("copy path:%s -> %s", copyInfo->src_dir, copyInfo->dst_dir);						
						
						memset(msgAck, 0, 512);
						msgAck->event = ENU_LC_DVR_GB19056_FILE_COPY;    //47
						memcpy(msgAck->msg_buf, copyInfo, sizeof(GBT19056_COPY_INFO));												

						fd = lc_socket_app_fd_get(LC_DVR_RECO_SERID);
						lc_socket_client_send(fd, msgAck, sizeof(LC_DVR_RECORD_MESSAGE)-16+sizeof(GBT19056_COPY_INFO));
					}
					break;
				case ENU_LC_DVR_DYNAMIC_NOTIFY:
					{
						char *dir = message->msg_ptr;
						logd_lc("dynamic mount device -> %s", dir);
						strcpy(vc_dynamic_mount_path(), dir);
					}
					break;				
				case ENU_LC_DVR_GB19056_VIDEO_COPY:
					{
						char *dir = vc_dynamic_mount_path();
						pthread_t video_cpy_thread;
						int ret = pthread_create(&video_cpy_thread, NULL, (void*)vc_video_file_copy_thread, dir);
					    if (0 == ret)
					    {
					        logd_lc("video copy pthread SUCCESS\n");
					        pthread_detach(video_cpy_thread);
					    }
					}
					break;
				case ENU_LC_DVR_GB19056_VIDEO_COPY_BREAK:
					{
						vc_video_file_copy_break();
					}
					break;
				case ENU_LC_DVR_RECORD_FILE_INFO_SAVE:
					{
						Record_fileInfo_dbsave();
					}
					break;
				case ENU_LC_DVR_CAM_DEV_ERROR: /**摄像头断开告警**/
					{
						logd_lc("ENU_LC_DVR_CAM_DEV_ERROR->%d", message->value);
						lc_dvr_camErr_notify(message->value);
					}
					break;
				case ENU_LC_DVR_STORAGE_ERROR: /**存储器故障告警**/
					{
						logd_lc("ENU_LC_DVR_STORAGE_ERROR->%d", message->value);
						lc_dvr_storageErr_notify(message->value);
					}
					break;
				case ENU_LC_DVR_FINISH_EMERGE_FILE:
					{
						logd_lc("ENU_LC_DVR_FINISH_EMERGE_FILE->%d", message->value);
					}
					break;
				case ENU_LC_DVR_SOUND_RECORD_START:
					{
						int duration, conti;
						logd_lc("ENU_LC_DVR_SOUND_RECORD_START->%x", message->value);
						duration = message->value&0x7fff;
						conti = (message->value&0x8000)?1:0;
						// lc_dvr_sound_record_start(message->value, 1);
						lc_dvr_sound_record_start(duration, conti);
					}					
					break;
				case ENU_LC_DVR_SOUND_STREAM_ON_OFF:
					if(message->value)
						lc_audio_export_start();
					else
						lc_audio_pipe_quit();
					break;
					
				case ENU_LC_DVR_SPECIAL_EMERGENCY_VIDEO:
					lc_dvr_emergency_file_build(message->value);
					break;
				case ENU_LC_DVR_NORMAL_EMERGENCY_VIDEO:
					lc_dvr_emergency_file_build(0);
					break;
				case ENU_LC_DVR_EMERGENCY_SPACE_OVERLIMIT:
					{
						lc_dvr_mcu_emergency_size(message->value);
					}
					break;
				case ENU_LC_DVR_EMERGENCY_STORAGE_LIMIT:
					{
						int rate = message->value;
						logd_lc("ENU_LC_DVR_EMERGENCY_STORAGE_LIMIT->%d", rate);
						if((rate>0)&&(rate<100))
						{
							lc_dvr_set_storage_limit(rate);
						}
					}
					break;
				case ENU_LC_DVR_IMAGE_LOOP_TAKE:
					{
						logd_lc("ENU_LC_DVR_IMAGE_LOOP_TAKE");
						vc_loop_image_ctrl(message->msg_buf);
					}
					break;
				case ENU_LC_DVR_FILE_LOCK_ON_OFF:
					{
						char *fileName = (char*)message->msg_ptr;
						logd_lc("ENU_LC_DVR_FILE_LOCK_ON_OFF");
						if(message->value)
						{
							LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(128);
							int fd = MSG_SRCID(message->msg_id);
							int retval;
							
							memset(msgAck, 0, 128);
							msgAck->event = ENU_LC_DVR_FILE_LOCK_ACK;
							retval = Record_File_Lock(fileName, msgAck->msg_ptr);	
							msgAck->value = retval;							
							lc_socket_client_send(fd, msgAck, 128);
						}
						else
						{
							Record_File_unLock(fileName);
						}
					}
					break;
				case ENU_LC_DVR_FETCH_FILE_BY_ID:
					{
						int* fileID = (int*)message->msg_buf;
						int fd = MSG_SRCID(message->msg_id);

						LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(256);
						msgAck->event = ENU_LC_DVR_FETCH_FILE_ID_ACK;
						logd_lc("ENU_LC_DVR_FETCH_FILE_BY_ID->%x", fileID[0]);
						
						memset(msgAck, 0, 256);

						if(Record_Fetch_Path_By_ID(fileID[0], msgAck->msg_buf)==0)
						{
							GBT19056_FILE_DETAIL_INFO *fileInfo = (GBT19056_FILE_DETAIL_INFO*)msgAck->msg_buf;
							logd_lc("fetch is:%s", fileInfo->fileName);
						}						
						lc_socket_client_send(fd, msgAck, 256);
					}
					break;
				case ENU_LC_DVR_RECORD_PARA_CONFIG:
					{
						vc_record_para_set(message->msg_buf);
					}
					break;
#ifdef LC_MDVR_FUNCTION_TEST
				case ENU_LC_DVR_FETCH_RECORD_LIST_ACK:/**回传的文件列表数据**/
					{
						LC_RECORD_LIST_FETCH *fileFetch = (LC_RECORD_LIST_FETCH*)message->msg_ptr;
						logd_lc("ENU_LC_DVR_FETCH_RECORD_LIST_ACK");
						{
							char *s8Ptr = fileFetch->fileList;
							char realPath[256];
							logd_lc("Ack-->camChnl:%d, fileNumber:%d", fileFetch->camChnl, fileFetch->fileNumber);
							logd_lc("last file index = %d", fileFetch->offset);
							for(int i=0; i<fileFetch->fileNumber; i++)
							{
								sprintf(realPath, "%s%s", fileFetch->fileDir, s8Ptr);
								s8Ptr += fileFetch->bytePerFile;
								logd_lc("record[%d]-->%s", i, realPath);
							}
						}						
					}
					break;				
				case ENU_LC_DVR_FETCH_CUR_RECORD_ACK:/**回传的当前录制文件的信息**/
					{
						LC_RECORD_LIST_FETCH *fileCur = (LC_RECORD_LIST_FETCH*)message->msg_ptr;
						char realPath[256];
						logd_lc("ENU_LC_DVR_FETCH_CUR_RECORD_ACK");
						logd_lc("Ack-->camChnl:%d, fileNumber:%d", fileCur->camChnl, fileCur->fileNumber);
						sprintf(realPath, "%s%s", fileCur->fileDir, fileCur->fileList);
						logd_lc("Cur record-->%s", realPath); /**获取到当前录制文件的序号后，再以该序号做为参考来获取其他文件**/
						
						{/**获取到当前录制文件的信息后再获取其他的文件列表**/	
							LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(256);
							LC_RECORD_LIST_FETCH *fileFetch = (LC_RECORD_LIST_FETCH*)msgAck->msg_buf;
							msgAck->msg_id = MSG_BUILD(LC_DVR_VIDEO_SERID,LC_DVR_RECO_SERID);
							
							msgAck->event = ENU_LC_DVR_FETCH_RECORD_LIST;
							msgAck->value = 0;

							fileFetch->camChnl = fileCur->camChnl; /**摄像头通道选择**/
							fileFetch->offset = fileCur->offset;  /**文件的起始序号，从起始序号开始顺序搜索fileNumber个可用文件**/
							fileFetch->fileNumber = 5;	/**需要获取文件的数量**/

							logd_lc("Test record list fetch!");
							lc_socket_client_send(lc_socket_client_fd(), msgAck, sizeof(LC_RECORD_LIST_FETCH)+16);								
						}

					}
					break;
				
				case ENU_LC_DVR_MSG_FOR_TEST:
					{
						switch(message->value)
						{
							case 0: /**测试获取当前录像文件信息**/
								{
									LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(sizeof(LC_DVR_RECORD_MESSAGE));
									
									msgAck->msg_id = MSG_BUILD(LC_DVR_VIDEO_SERID,LC_DVR_RECO_SERID);
									msgAck->event = ENU_LC_DVR_FETCH_CUR_RECORD;
									msgAck->value = 3; /**指定需要获取的摄像头通道**/ 
									
									logd_lc("Test fetch current record file!");
									lc_socket_client_send(lc_socket_client_fd(), msgAck, sizeof(LC_DVR_RECORD_MESSAGE));
								}
								break;
							case 1:/**测试推流功能启动**/
								{
									LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(128);
									LC_VENC_STREAM_INFO* streamInfo = (LC_VENC_STREAM_INFO*)msgAck->msg_buf;
									
									msgAck->msg_id = MSG_BUILD(LC_DVR_VIDEO_SERID,LC_DVR_RECO_SERID);
									msgAck->event = ENU_LC_DVR_STREAM_OPEN;
									msgAck->value = mFunTestCnt;
									streamInfo->resolution = STREAM_1280X720_ENU;
									//streamInfo->resolution = STREAM_960X544_ENU;
									//streamInfo->resolution = STREAM_1920X1080_ENU;
									streamInfo->camChnl = 5; //mFunTestCnt;
									logd_lc("Test[%d] stream push!-->STREAM_1920X1080_ENU", mFunTestCnt);
									lc_socket_client_send(lc_socket_client_fd(), msgAck, sizeof(LC_VENC_STREAM_INFO)+16);
									mFunTestCnt++;
								}
								break;
							case 2:/**测试推流通道切换**/
								{
									LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(sizeof(LC_DVR_RECORD_MESSAGE));
									
									msgAck->msg_id = MSG_BUILD(LC_DVR_VIDEO_SERID,LC_DVR_RECO_SERID);
									msgAck->event = ENU_LC_DVR_STREAM_CHANEL;
									msgAck->value = 1; /**需要切换的通道**/	
									
									logd_lc("Test stream chanel change!");
									lc_socket_client_send(lc_socket_client_fd(), msgAck, sizeof(LC_DVR_RECORD_MESSAGE));							
								}								
								break;
							case 3:/**测试推流功能关闭，不同的分辨率需要先关闭再重新开启**/
								{
									LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(sizeof(LC_DVR_RECORD_MESSAGE));
									
									msgAck->msg_id = MSG_BUILD(LC_DVR_VIDEO_SERID,LC_DVR_RECO_SERID);
									msgAck->event = ENU_LC_DVR_STREAM_CLOSE;
									msgAck->value = 0; /**需要切换的摄像头通道**/	
									
									logd_lc("Test stream turn off!");
									lc_socket_client_send(lc_socket_client_fd(), msgAck, sizeof(LC_DVR_RECORD_MESSAGE));							
								}								
								break;							
							case 4:/**测试拍照功能**/
								{
									LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(256);
									LC_TAKE_PIC_INFO *picInfo = (LC_TAKE_PIC_INFO*)msgAck->msg_buf;

									msgAck->msg_id = MSG_BUILD(LC_DVR_VIDEO_SERID,LC_DVR_RECO_SERID);
									msgAck->event = ENU_LC_DVR_MDVR_TAKEPIC;
									msgAck->value = 0; /**需要切换的摄像头通道**/	
									
									picInfo->pic_width = LC_TAKE_LARGE_PIC_SIZE_W;
									picInfo->pic_height = LC_TAKE_LARGE_PIC_SIZE_H;
									picInfo->camChnl = 3; /**拍照的摄像头通道选择**/
									sprintf(picInfo->pic_save_path, "%s/WondPic1.jpg", JPG_TEST_DIR);
									
									logd_lc("Test picture take!");
									lc_socket_client_send(lc_socket_client_fd(), msgAck, sizeof(LC_TAKE_PIC_INFO)+16);								
								}								
								break;
							case 5:/**测试视频回放**/
								{
									LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(sizeof(LC_DVR_RECORD_MESSAGE)+128);
									char *mediaFile = (char*)msgAck->msg_buf;
									msgAck->msg_id = MSG_BUILD(LC_DVR_VIDEO_SERID,LC_DVR_RECO_SERID);
									msgAck->event = ENU_LC_DVR_MEDIA_PLAY_START;
									msgAck->value = 0; /**需要切换的摄像头通道**/	

									strcpy(mediaFile, "/mnt/nfs/mediaTmp/test.mp4");
									logd_lc("Test media play!");
									lc_socket_client_send(lc_socket_client_fd(), msgAck, sizeof(16+128));							
								}								
								break;
							case 6:/**测试视频倍速播放**/
								break;
							case 7:/**测试视频指定时间戳播放**/
								break;
							case 8:/**测试视频播放停止**/
								break;	
							case 9:
								{		
									int8_t *strPtr;				
									LC_DVR_RECORD_MESSAGE *msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(512);
									GBT19056_COPY_INFO    *copyInfo;
								    time_t tt = time(NULL);
								    struct tm *timer = localtime(&tt);
								    int8_t  dstDir[128];
									int32_t position;
									
									logd_lc("ENU_LC_DVR_SD_GET_RECORD_FILE_PATH");
									
									memset(msgAck, 0, sizeof(LC_DVR_RECORD_MESSAGE));

									msgAck->event = ENU_LC_DVR_GB19056_FILE_COPY;    //47
									copyInfo     = msgAck->msg_buf;
									memset(copyInfo, 0, sizeof(GBT19056_COPY_INFO));

									timer->tm_year -= 100;
									timer->tm_mon  += 1;
									position  = sprintf(dstDir,          "DIR%x%x",   timer->tm_year/10, timer->tm_year % 10);
									position += sprintf(dstDir + position, "%x%x",    timer->tm_mon /10, timer->tm_mon  % 10);
									position += sprintf(dstDir + position, "%x%x",    timer->tm_mday/10, timer->tm_mday % 10);
									
									position += sprintf(dstDir + position, "_%x%x",   timer->tm_hour/10, timer->tm_hour % 10);
									position += sprintf(dstDir + position, "%x%x",    timer->tm_min /10, timer->tm_min  % 10);
									position += sprintf(dstDir + position, "%x%x",    timer->tm_sec /10, timer->tm_sec  % 10);	
									
									if (access(dstDir, F_OK) != F_OK) {
								        uint8_t buf[32] = { 0 };
								        sprintf(buf, "mkdir /data/%s", dstDir);
								        system(buf);
								    }
												
									//startTime[8][6]; /**指定始时间的BCD码，全0则从最开始的记录时刻开始**/
									//stopTime[8][6];  /**指结束时间的BCD码，全0则从最最末的记录时刻结束**/
									sprintf(copyInfo->dst_dir, "/data/%s", dstDir);
									copyInfo->fetchList[0] = TRAVEL_RECORD;
									copyInfo->fetchList[1] = DOUBTFUL_RECORD;
									copyInfo->fetchList[2] = OVER_TIME_RECORD;
									copyInfo->fetchList[3] = DRIVER_RECORD;
									copyInfo->fetchList[4] = DAILY_RECORD;
									copyInfo->callback     = NULL;  //(void*)lc_dvr_after_copy;
									lc_socket_client_send(lc_socket_client_fd(), msgAck, sizeof(LC_DVR_RECORD_MESSAGE)-16+sizeof(GBT19056_COPY_INFO));
								}									
								break;
						}						
					}
					break;
#endif				
			}
		}
	}
}

