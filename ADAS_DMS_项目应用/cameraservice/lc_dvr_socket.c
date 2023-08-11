#include <netinet/in.h> /**INADDR_ANY, IPPROTO_TCP**/
#include <pthread.h>    /**PTHREAD_CREATE_DETACHED**/
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>      /**errno**/
#include <string.h>     /**memset**/
#include <errno.h>
#include "lc_dvr_record_log.h"
#include "lc_dvr_socket.h"
#include "lc_dvr_record_sys_func.h"

#define LC_CLENT_MAX_NUM 	32
#define SERVER_IP_PORT  	(7680)
#define MAX_RECV_BUFF   	(512)
#define LISTEN_BACKLOG  	(4)
#define SOCKET_SERVER_REC_BUF_LEN 	4*1024
#define SOCKET_CLIENT_REC_BUF_LEN 	4*1024
#define SOCKET_IDENTIFY_CODE 0X55AAEFCD

static int  mClient_fd[LC_CLENT_MAX_NUM];
static int  mListen_fd;
static int  mCur_fd[16];
static int  mApp_fd[16];
static int  mCufFdCnt=0;
static int  mSocketRun = 1;
static int  mCurrentClientFd = -1;
static pthread_mutex_t mSocketMutex;
static char *mSocketRecBuf = NULL;

void lc_socket_app_fd_set(int appId, int fd)
{
	logd_lc("app_fd_set[%x]->%d", appId, fd);
	mApp_fd[appId&0x0f] = fd;
}

int lc_socket_app_fd_get(int appId)
{
	logd_lc("app_fd_get[%x]->%d", appId, mApp_fd[appId&0x0f]);
	return mApp_fd[appId&0x0f];
}

void lc_client_insert(int fd)
{
	for(int i=0; i<LC_CLENT_MAX_NUM; i++)
	{
		if(mClient_fd[i]<0) 
		{
			mClient_fd[i] = fd;
			logd_lc("client(%d) insert at %d\n", fd, i);
			break;
		}
	}
}

void lc_client_remove(int fd)
{
	int i;
	
	for(i=0; i<LC_CLENT_MAX_NUM; i++)
	{
		if(mClient_fd[i]==fd) 
		{
			logd_lc("client(%d) remove at %d\n", fd, i);
			break;
		}
	}

	for(;i<LC_CLENT_MAX_NUM-1; i++)
	{
		mClient_fd[i] = mClient_fd[i+1];
	}

	mClient_fd[i] = -1;
}

void lc_dvr_socket_send_msg(int eMsg, int val, void* para, void* func)
{
	LC_DVR_RECORD_MESSAGE message;
	message.ackFunc = NULL;
	message.msg_id  = MSG_BUILD(LC_DVR_SYS_SERID, LC_DVR_RECO_SERID);
	message.event = eMsg;
	message.value = val;
	message.msg_ptr = para;
	message.ackFunc = (LC_ACK_FUNC)func;
	sys_msg_queu_in(&message);
}

void lc_client_init(void)
{
	for(int i=0; i<LC_CLENT_MAX_NUM; i++)
		mClient_fd[i] = -1;
}

void clientAckFun(int key, int para)
{
	switch(key)
	{
		case ENU_LC_DVR_SD_GET_VIDEO_FILE_PATH:
			{
				
			}
			break;
	}
}

extern void lc_dvr_mem_debug(void* membuf, int len);

int lc_socket_data_parse(int fdID, char* input, int *len)
{
	uint32_t *u32Ptr = (uint32_t*)input;
	int wLen = len[0];
	int retval = 0;
	
	if(wLen > 8)
	{
		if(u32Ptr[0]==SOCKET_IDENTIFY_CODE)
		{
			int wantLen, dataLen;
			char *dstPtr, *srcPtr;
			dataLen = u32Ptr[1];
			wantLen = dataLen+8;
	
			if(wLen>=wantLen)
			{
				
				char *extBuf = (char*)sys_msg_get_memcache(dataLen);
				LC_DVR_RECORD_MESSAGE *recv_msg = (LC_DVR_RECORD_MESSAGE*)extBuf;
				memcpy(extBuf, (input+(sizeof(int)*2)), dataLen);
				recv_msg->msg_id = MSG_BUILD(fdID, LC_DVR_VIDEO_SERID);
				recv_msg->msg_ptr = recv_msg->msg_buf;
				recv_msg->ackFunc = clientAckFun;		
				sys_msg_queu_in(recv_msg);
	
				logd_lc("video msget->%d(%d)!", recv_msg->event, recv_msg->value);
				dstPtr = input;
				srcPtr = input+wantLen;
				
				for(int i=0; i<(wLen-wantLen); i++)
				{
					*dstPtr++ = *srcPtr++;
				}
				
				len[0] -= wantLen;
				retval = 1;
			}
		}
		else
		{
			len[0] = 0;
			logd_lc("no socke vpacket!");
		}
	}

	return retval;
}

void *lc_socket_client_thread(void *arg)
{	
	char *msg_buf = (char*)malloc(SOCKET_CLIENT_REC_BUF_LEN);
	int cur_len;
	// create socket
	logd_lc("socket_client_thread!!\n");
	
	int clientfd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == clientfd) {
		loge_lc("create socket error");
		return NULL;
	}
	
	// connect server 
    struct sockaddr_in serv_addr;
    memset((void *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_IP_PORT);

	while(mSocketRun)
	{
		if(-1 == connect(clientfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))){
			sleep(1);
			continue;
		}
		logd_lc("connect to server success!\n");
		break;
	}

	mCurrentClientFd = clientfd; /**通知系统录像进程已经开始了，把mount结果汇报过去**/
	lc_dvr_socket_send_msg(ENU_LC_DVR_SOCKET_CONNECT, 0, NULL, NULL);
	cur_len = 0;
	
	while(mSocketRun)
	{
		int recv_len = -1;
		recv_len = recv(clientfd, msg_buf+cur_len, SOCKET_CLIENT_REC_BUF_LEN, 0);
		if (recv_len < 0)
		{
			fprintf(stderr, "client recv error %s errno: %d\n", strerror(errno), errno);
			continue;
		}
	
		if (0 == recv_len)
		{
			logd_lc("client has closed!\n");
			break;
		}
		
		pthread_mutex_lock(&mSocketMutex);
		cur_len += recv_len;
		logd_lc("cli(%d) rec_len = %d",clientfd, recv_len);
		while(lc_socket_data_parse(clientfd, msg_buf, &cur_len))
		{
			;
		}
		printf("cpacket end!");
		pthread_mutex_unlock(&mSocketMutex);
	}
	
	close(clientfd);
	free(msg_buf);
	logd_lc("client socket thread quit!");

    return NULL;
}

void* lc_socket_server_handler(void* arg)
{
	int recv_len, cur_len;	
	int *s32_Ptr = (int*)arg;
	int conn_fd = s32_Ptr[0];
	char *msg_buf = (char*)malloc(SOCKET_SERVER_REC_BUF_LEN);
	char *msgTemp = NULL;

	lc_client_insert(conn_fd);
	logd_lc("Client(%d) start!!\n", conn_fd);
	cur_len = 0;
	while (mSocketRun)
	{
		recv_len = -1;
		recv_len = recv(conn_fd, msg_buf+cur_len, SOCKET_SERVER_REC_BUF_LEN, 0);
		if (recv_len < 0)
		{
			fprintf(stderr, "server recv error %s errno: %d\n", strerror(errno), errno);
			if(errno == 9) //server recv error Bad file descriptor errno: 9 /**多次重连会报该错误**/
				break;
			else
				continue;
		}

		if (0 == recv_len)
		{
			break;
		}
		
		pthread_mutex_lock(&mSocketMutex);
		logd_lc("ser(%d) rec_len = %d",conn_fd, recv_len);
		cur_len += recv_len;
		while(lc_socket_data_parse(conn_fd, msg_buf, &cur_len))
		{
			;
		}		
		printf("spacket end!\n");
		pthread_mutex_unlock(&mSocketMutex);
	}
	
	close(conn_fd);
	lc_client_remove(conn_fd);
	free(msg_buf);
	logd_lc("server socket thread quit!\n");

	return NULL;
}

/**线程，用来阻塞sock，监听并等待连接**/
void* lc_sock_accept_thread(void* arg)
{
	int ret = -1;
	int one = 1;
	struct sockaddr_in serv_addr;
	memset((void *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(SERVER_IP_PORT);

	mListen_fd = -1;
	mSocketRun = 1;
	for(int i=0; i<16; i++) mApp_fd[i] = -1;

	pthread_mutex_init(&mSocketMutex, NULL);
	
	logd_lc("lc_socket_noblock!");

	lc_client_init();
	mListen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mListen_fd < 0) {
		loge_lc("socket error-->%d!!\n", mListen_fd);
	}
	
	if (setsockopt(mListen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
		close(mListen_fd); /**socket关闭后会进入time_wait状态，如果需要快速重启则在bind的时候会返回-1，配置成SO_REUSEADDR**/
		return NULL;		/**就可以让端口在释放后就可以使用，避免bind失败**/
	}

	ret = bind(mListen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (ret < 0) {
		loge_lc("fd[%d] bind socket error-->%x!!\n", mListen_fd, ret);
	}

	ret = listen(mListen_fd, LISTEN_BACKLOG);
	if (ret < 0) {
		loge_lc("listen error!!\n");
	}
	
	int flags = fcntl(mListen_fd, F_GETFL, 0);		/**设置成非阻塞方式**/
	fcntl(mListen_fd, F_SETFL, flags | O_NONBLOCK);

	logd_lc("noblock sock_accept_thread run......\n");
	while (mSocketRun)
	{
		int conn_fd = -1;
		conn_fd = accept(mListen_fd, (struct sockaddr*)NULL, NULL);
		if (conn_fd<0)
		{
			if (errno==EAGAIN || errno == EWOULDBLOCK)
			{
				usleep(200*1000);
				continue;
			}
			else
			{
				perror("call accept error");
				break;
			}
		} 
		else
		{
			pthread_t serv_thread;
			logd_lc("client_1 is connect!\n");
			mCur_fd[mCufFdCnt] = conn_fd;
			ret = pthread_create(&serv_thread, NULL, (void*)lc_socket_server_handler, &mCur_fd[mCufFdCnt]);
			
			if (0 == ret)
			{
				logd_lc("client[%d] service success -->%d!\n",mCufFdCnt, mCur_fd[mCufFdCnt]);
				pthread_detach(serv_thread);
			}
			
			mCufFdCnt++;
			mCufFdCnt&=0x0f;
		}
	}	
	
	close(mListen_fd);
	logd_lc("monitor socket thread quit!");
	return NULL;
}


int  lc_socket_client_fd(void)
{
	return mCurrentClientFd;
}

void lc_socket_client_send(int fd, void* data, int len)
{
	//logd_lc("video (%d) send %d byte\n", fd, len);
	uint32_t u32Ptr[2];
	u32Ptr[0] = SOCKET_IDENTIFY_CODE;
	u32Ptr[1] = len;
	send(fd, u32Ptr, (sizeof(int)*2), 0);
	
	// lc_dvr_mem_debug(data, len);

	send(fd, data, len, 0);
}

void lc_socket_get_video_dir(int fd)
{
	LC_DVR_RECORD_MESSAGE* msgAck = (LC_DVR_RECORD_MESSAGE*)sys_msg_get_memcache(sizeof(LC_DVR_RECORD_MESSAGE));
	memset(msgAck, 0, sizeof(LC_DVR_RECORD_MESSAGE));
	msgAck->msg_id = MSG_BUILD(LC_DVR_VIDEO_SERID,LC_DVR_RECO_SERID);
	msgAck->event = ENU_LC_DVR_AV_MANAGER;
	msgAck->value = ENU_LC_DVR_SD_GET_VIDEO_FILE_PATH;
	logd_lc("lc_socket_get_video_dir\n");
	lc_socket_client_send(fd, msgAck, sizeof(LC_DVR_RECORD_MESSAGE));
}

void lc_socket_exit(void)
{
	mSocketRun = 0;	
	for(int i=0; i<LC_CLENT_MAX_NUM; i++)
	{
		if(mClient_fd[i]>=0) shutdown(mClient_fd[i], SHUT_RDWR);
	}

	if(mCurrentClientFd>=0)
	{
		shutdown(mCurrentClientFd, SHUT_RDWR);
	}		
	usleep(300*1000);	
	logd_lc("quit socket......");
}
