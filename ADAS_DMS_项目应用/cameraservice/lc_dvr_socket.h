#ifndef _LC_DVR_SOCKET_H_
#define _LC_DVR_SOCKET_H_
#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

typedef enum UI_CTRL_CMD
{
    CMD_PREVIEW_START,
    CMD_PREVIEW_MODE_CHANGE,
    CMD_RECORD_CTRL,

    /**褰曞儚鍥炴斁鎺у埗鍛戒护**/
    CMD_PLAYER_CTRL_START,
    CMD_PLAYER_CTRL_PAUSE,
    CMD_PLAYER_CTRL_STOP,
    CMD_PLAYER_CTRL_PREV,
    CMD_PLAYER_CTRL_NEXT,
    CMD_PLAYER_CTRL_BACKWARD,
    CMD_PLAYER_CTRL_FORWARD,
    CMD_PLAYER_FILE_SIZE,
    CMD_PLAYER_PROGRESS_STATUS, /**鎾斁瀹炴椂鐘跺喌**/
    /**系统事件**/
	CMD_EVENT_CARD_INSERT,
	CMD_EVENT_USB_INSERT,
	CMD_EVENT_SOCKET_QUIT,
    STA_CMD_NC
}UI_CTRL_CMD_E;

void lc_socket_app_fd_set(int appId, int fd);
int  lc_socket_app_fd_get(int appId);
void *lc_socket_client_thread(void *arg);
void* lc_socket_server_handler(void* arg);
void* lc_sock_accept_thread(void* arg);
int  lc_socket_client_fd(void);
void lc_socket_client_send(int fd, void* data, int len);
void lc_socket_send_recordList_ack(int fd, void* msg, int length);
void lc_socket_get_video_dir(int fd);
void lc_socket_exit(void);
void *lc_socket_noblock(void* arg);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif

