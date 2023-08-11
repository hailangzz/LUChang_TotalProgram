#ifndef _LC_DVR_MEDIA_DEMUX_H
#define _LC_DVR_MEDIA_DEMUX_H
extern int media_demux_start(char *fileName);
extern void media_demux_stop(int fp);
extern void media_demux_set_speed(int speed);
extern void mddia_data_pro_register(void *funcion);	/**注册数据的回调函数**/
#endif
