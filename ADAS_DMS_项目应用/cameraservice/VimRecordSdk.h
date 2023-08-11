#ifndef _VIMRECORDSDK_H_
#define _VIMRECORDSDK_H_
#include"VimMediaType.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief vimicro sysapi init
 * @note 此接口也可以自己使用vimicro sys api实现，在所有接口调用前使用
 */
int VIMSysInit(void);

/**
 * @brief vimicro sysapi exit
 *
 */
void VIMSysExit(void);

/******************视频编码模块相关接口**********************/
/**
 * @brief 启动视频编码通道
 * @param chId 输入参数，用户指定编码通道号，范围：MIN_VENC_CH~MAX_VENC_CH
 * @param fmt 输入参数，指定编码器编码类型，目前仅支持AV_FMT_H265和AV_FMT_SVAC2
 * @param vInfo 输入参数，指定编码通道的视频输入参数
 * @param bitRate 输入参数，指定编码码率
 * @param type 输入参数，用户指定编码通道源，类型为：RECORD_SOURCE（查看VimMediaType.h）
 * @param srcMod 输入参数，当type为BIND_VPSS_TYPE时，指定vpss的通道信息，sdk内部绑定，如果为NULL，需要用户自己绑定
 * @param useVencBuf 输入参数，是否需要编码通道申请输入帧缓存，
 * 当type为PUSH_FRAME_TYPE时固定为1，否则要根据绑定输入模块vpss的输出通道参数u32VoIsVpssBuf而定
 * @note 生成的录像文件名格式如：ch0_20220101_000446998.mp4，ch0就是代表通道0，跟接口的第一参数关联
 * @return 成功：0，错误码：1：没有初始化、2：通道号错误、3：通道已经启动、4：启动录像线程失败、5：启动视频编码器失败
 */
int VimVencChStart(int chId, AV_CODEC_FMT fmt, const VFrameInfo* vInfo, int bitRate, int type, const VimModule* srcMod, int useVencBuf);

/**
 * @brief 向编码通道推送一帧YUV数据
 * @param chId 输入参数，用户指定编码器通道号
 * @param data 输入参数，YUV数据内存地址
 * @param len 输入参数，YUV数据长度
 * @param pts 输入参数，视频帧时间戳，单位为毫秒
 * @param copy 输入参数，是否拷贝yuv数据，为0时，表示不拷贝，data内存地址必须为malloc函数申请，并且只有当接口返回失败用户才需要free释放此内存，否则有sdk内部释放
 * @note 只有当VimVencChStart的type为PUSH_FRAME_TYPE类型时才需要使用此接口
 * @return 成功：0，错误码：-1：参数错误、1：内部队列已满、2：通道没有启动
 */
int VimVencChPushVFrame(int chId, const char* data, int len , long long pts, int copy);

/**
 * @brief 添加视频编码器输出帧数据回调
 * @param chId 编码器通道号
 * @param cb 回调接口
 * @param userData 用户数据
 * @note
 * @return 成功：0，错误码：1：通道不存在
 */
int VimVencChAddConsumer(int chId, VencPacketCb cb, void* userData);

/**
 * @brief 删除编码器输出回调
 * @param ch 输入参数，通道号
 * @param cb 回调接口
 * @note
 * @return 成功：0，错误码：1：通道不存在
 */
int VimVencChDelConsumer(int chId, VencPacketCb cb);

/**
 * @brief 停止视频编码通道
 * @param ch 输入参数，通道号
 * @note
 * @return 成功：0，错误码：1：通道不存在
 */
int VimVencChStop(int chId);


/******************视频转码模块相关接口**********************/
/**
 * @brief 打开一个视频转码器
 * @param srcFmt 输入参数，源编码格式
 * @param dstFmt 输入参数，目标编码格式
 * @param cb 输入参数，输出回调接口
 * @param userData 输入参数，用户数据
 * @note 目前仅支持SVAC2转H265，最多创建4个编码器
 * @return 成功返回：大于MAX_VENC_CH的通道号(为兼容上面编码器回调接口)，错误码：-1：参数错误，-2：已达到最大通道数
 */
int VimTranscodeOpen(AV_CODEC_FMT srcFmt, AV_CODEC_FMT dstFmt, int bitRate, VencPacketCb cb, void* userData);

/**
 * @brief 向转码通道推送一帧已编码数据
 * @param ch 输入参数，用户指定转码通道号
 * @param fmt 输入参数，编码帧数据类型
 * @param data 输入参数，已编码帧数据内存地址
 * @param len 输入参数，帧数据长度
 * @param pts 输入参数，视频帧显示时间戳，单位为毫秒
 * @param dts 输入参数，视频帧编码时间戳，单位为毫秒
 * @param flag 输入参数，帧类型标志，0：为非关键帧，其它值为关键帧
 * @note
 * @return 成功：0，错误码：1：参数错误、2：没有对应通道、3：转码队列已满、4：转码错误
 */
int VimTranscodePushPacketData(int ch, AV_CODEC_FMT fmt, const unsigned char* data, int len , long long pts, long long dts, int flag);

/**
 * @brief 停止转码通道
 * @param ch 输入参数，通道号
 * @note
 * @return 成功：0，错误码：1：通道不存在
 */
int VimTranscodeClose(int ch);


/******************录像模块相关接口**********************
录像模块支持内部直接绑定上面的编码模块，停止时自动解绑，
也可以不用上面的编码模块，自己实现编码然后使用VimRecordChPushPacketData推送已编码数据录像
*/

/**
 * @brief 录像sdk初始接口
 * @param recordDir 指定录像文件存放路径，路径必须存在且剩余空间大于200M，当需要SDK内部自动创建录像文件名并管理时需要指定此参数
 * @param cb 动态获取录像文件全路径回调函数，当用户需要自己管理录像文件时需要实现此函数，此时会忽略recordDir参数
 * @param handler 录像和回放支持外部读写接口
 * @param initHawk 是否启动hawksdk，0:标识不启动，其它值为启动，如果需要svac加解密则必须启动
 * @param pwdMd5Str 输入参数，录制加密录像时需要提供录像密码的md5加密字符串，目前仅支持视频流svac2的加密，即参数info->vFmt为AV_FMT_SVAC2且info->vEncConfig为非0时，必须提供次参数
 * @param param 输入参数，viss平台GB28181协议接入参数，如果需要接入viss平台需要此参数配置GB28181接入配置，并且initHawk必须开启
 * @note 必须在其他录像接口调用前使用，只需要调用一次
 * @return 成功：0，错误码：1：已经初始化、2：无效的参数、3：路径空间不足、4：启动线程出错、5：启动hawk出错
 */
int VimRecordInit(const char* recordDir, GetRecFullPathCb cb, ExtRwHanlder* handler, int initHawk, const char* pwdMd5Str, const VissGbParam* param);

/**
 * @brief 启动录像通道
 * @param ch 输入参数，用户指定录像通道号，范围：MIN_REC_CH~MAX_REC_CH
 * @param bindVencCh 输入参数，大于等于0时内部绑定视频编码通道号，小于0需要使用VimRecordChPushPacketData推送视频帧录像
 * @param info 输入参数，指定录像通道媒体参数
 * @note 生成的录像文件名格式如：ch0_20220101_000446998.mp4，ch0就是代表通道0，跟接口的第一参数关联
 * @return 成功：0，错误码：1：没有初始化、2：通道号错误、3：通道已经启动、4:录像参数错误，5：启动录像线程失败、6:创建加密通道失败，7：绑定视频编码器失败
 */
int VimRecordChStart(int ch, int bindVencCh, const MediaInfo* info);

/**
 * @brief 向录像通道推送一帧已编码数据
 * @param ch 输入参数，用户指定录像通道号
 * @param fmt 输入参数，编码帧数据类型
 * @param data 输入参数，已编码帧数据内存地址
 * @param len 输入参数，帧数据长度
 * @param pts 输入参数，视频帧显示时间戳，单位为毫秒
 * @param dts 输入参数，视频帧编码时间戳，单位为毫秒
 * @param flag 输入参数，帧类型标志，0：为非关键帧，其它值为关键帧
 * @note 接口内部copy了帧数据，只有当VimRecordChStart的bindVencCh小于0时才需要次接口推送音视频帧录像
 * @return 成功：0，错误码：-1：参数错误、1：内部队列已满、2：通道没有启动
 */
int VimRecordChPushPacketData(int ch, AV_CODEC_FMT fmt, const unsigned char* data, int len, long long pts, long long dts, int flag);

/**
 * @brief 停止录像通道
 * @param ch 输入参数，用户指定录像通道号
 * @note
 * @return 成功：0，错误码：1：没有初始化、2：通道不存在
 */
int VimRecordChStop(int ch);

/**
 * @brief 开启通道事件录像，固定录制前后30s
 * @param ch 输入参数，用户指定录像通道号，必须是一个已经启动定时录像的通道
 * @param event 输入参数，用户指定录像事件类型，必须大于0
 * @note
 * @return 成功：0，错误码：1:没有初始化、2：通道不存在、3：通道没启动、4：此通道正在进行事件录像、5：创建事件录像出错、6：未知错误
 */
int VimStartEventRecord(int ch, int event);

/**
 * @brief 录像sdk反初始化
 * @note 必须先停止所有录像通道，才能调用此接口
 * @return 成功：0，错误码：1：录像通道没有停止
 */
int VimRecordUnInit(void);

/**
 * @brief 设置设备音频数据回调接口
 * @param cb 输入参数，音频回到函数
 * @param userData 输入参数，用户数据地址，可以为NULL
 * @note
 * @return 成功：0，错误码：无
 */
int VimSetDeviceAudioCb(AudioFrameCb cb, void* userData);

/******************录像回放相关接口**********************/

/**
 * @brief 启动播放器打开文件，获取录像文件信息
 * @param file 输入参数，需要播放的录像文件的完整路径
 * @param info 输出参数，接口调用成功时返回录像信息
 * @note 最多打开16个播放通道
 * @return 成功：大于0的播放句柄，错误码：0:播放通道已到上限，-1：打开文件失败，-2：文件格式不支持播放
 */
int VimPlayerOpen(const char* file, MediaInfo* info);

/**
 * @brief 启动录像文件播放
 * @param handle 输入参数，播放句柄
 * @param vo 输入参数，视频图像输出的vo或者vpss模块信息，调用接口前用户需要先创建好对应的模块通道
 * @param cb 输入参数，播放状态事件回调，包括：播放进度、播放结束、播放出错
 * @param pwdMd5Str 输入参数，播放加密视频流时需要提供播放密码，值为明文密码通过md5加密后32个字节的小写字符串；如果文件为非加密的输入NULL即可
 * @param userData 输入参数，用户参数
 * @note 
 * @return 成功：0，错误码：1：播放通道没打开，2:密码错误，3：创建解密通道失败，4：创建解码器出错，5：启动播放线程出错
 */
int VimPlayerStart(int handle, const VoModInfo* vo, PlayCallBack cb, const char* pwdMd5Str, void* userData);

/**
 * @brief 播放控制接口
 * @param handle 输入参数，播放句柄
 * @param cmd 输入参数，播放控制命令
 * cmd：PLAY_PAUSE_CMD和PLAY_START_CMD，data参数无意义，可以为NULL，
 * cmd: PLAY_SEEK_CMD时，data为double类型指针，指定跳转到录像指定时间点，单位为秒
 * cmd: PLAY_SPEED_CMD时，data为int类型指针，用来设置或者取消倍速播放，0为取消倍速播放，其它值为倍速播放
 * @param data 输入参数，控制命令参数，不同的cmd对应不同的参数类型:
 * @note
 * @return 成功：0，错误码：1：播放通道不存在，2：命令错误
 */
int VimPlayerCtrl(int handle, int cmd, void* data);

/**
 * @brief 关闭播放
 * @param handle 输入参数，播放句柄
 * @note
 * @return 成功：0，错误码：1：播放通道不存在
 */
int VimPlayerClose(int handle);

/**
 * @brief 字符串md5加密接口
 * @param inStr 输入参数，需要加密的字符串
 * @param outMd5Str 输出参数，输出md5加密后的字符串，为32个字节的小写字符，用户提供大于等于33个字节的内存地址
 * @note
 * @return 无
 */
void StringMd5Sum(const char* inStr, char* outMd5Str);

/**
 * @brief 启动rtsp服务器
 * @param port 输入参数，服务器端口
 * @param handler 输入参数，rtsp事件回调结构体
 * @note 客户端rtsp请求链接格式:rtsp://ip:port?param1&param2
?* param1:必选，文件绝对路径的base64编码
?* param2:可选，加密文件的密钥，如果是非加密文件，不需要传
 * @return 成功：0，错误码：1：启动失败
 */
int VimRtspStart(int port, RtspEventHandler handler);

/**
 * @brief 打开录像文件，创建解复用器，获取媒体信息
 * @param file 输入参数，文件全路径
 * @param pHandle 输出参数，返回一个demuxer句柄
 * @param info 输出参数，输出媒体信息
 * @note
 * @return 成功：0，错误码：1：参数错误，2：媒体文件格式错误，3：媒体格式不支持
 */
int VimMpsDemuxerOpenFile(const char *file, MPSHANDLE *pHandle, MediaInfo *info);

/**
 * @brief 从打开的录像文件中获取，视频加密相关的vkek和vkekver用于后面的解密视频帧数据
 * @param h 输入参数，demuxer句柄
 * @param pwdMd5Str 输入参数，录像文件加密密码的md5串
 * @param vkek 输出参数，输出16字节长度的加密key
 * @param vkekver 输出参数，输出长度为20字节的字符串key version
 * @note 播放加密录像文件时才需要使用此接口
 * @return 成功：0，错误码：1：参数错误，2：媒体文件格式错误，3：输入加密密码错误
 */
int VimMpsDemuxerGetVkek(MPSHANDLE h, const char* pwdMd5Str, char* vkek, char* vkekver);

/**
 * @brief 获取媒体数据包
 * @param h 输入参数，demuxer句柄
 * @param packet 输出参数， 成功返回媒体数据
 * @note
 * @return 成功：大于0，错误码：0：文件尾，-1：参数错误
 */
int VimMpsDemuxerReadPacket(MPSHANDLE h, MpsPacket *packet);

/**
 * @brief 释放掉VimMpsDemuxerReadPacket获取的数据包
 * @param h 输入参数，demuxer句柄
 * @param packet 输入参数， 通过VimMpsDemuxerReadPacket获取的数据
 * @note
 * @return 无
 */
void VimMpsDemuxerReleasePacket(MPSHANDLE h, MpsPacket *packet);

/**
 * @brief 文件位置跳转,跳指定的位置
 * @param h 输入参数，demuxer句柄
 * @param pos 输入参数，指定跳转位置，单位为秒
 * @note
 * @return 成功：0，错误码：1：无效的参数，2：seek位置pos无效
 */
int VimMpsDemuxerSeek(MPSHANDLE h, double pos);

/**
 * @brief 关闭已打开的解复用器
 * @note
 * @return 无
 */
void VimMpsDemuxerClose(MPSHANDLE h);


/******************流解密相关接口**********************/
/*目前只对SVAC2加密码流解密*/

/**
 * @brief 打开一个解密通道
 * @param vkek 输入参数，输入16字节长度的加密key，可以由接口VimMpsDemuxerGetVkek获取
 * @param vkekver 输入参数，key version
 * @note
 * @return 成功返回大于等于0的通道号，错误码：-1：参数错误，-2：解密模块未启动，-3：解密通道创建失败
 */
int VimStreamDecryptChOpen(const char* vkek, const char* vkekver);

/**
 * @brief 解密一帧视频数据
 * @param ch 输入参数，指定解密通道号，VimStreamDecryptChOpen返回值
 * @param srcData 输入参数，解密前数据缓存
 * @param len 输入参数，解密前数据长度
 * @param flag 输入参数，视频帧类型，0为非关键帧，其它值为关键帧
 * @param dstData 输出参数，解密后视频帧数据输出缓存，此内存由用户自己创建，且长度必须比解密前数据长度长256个字节；
 * @note 同步解密接口
 * @return 成功返回大于0的解密后数据长度，错误码：-1：参数错误，-2：解密模块未启动，-3：未找到解密通道，-4：解密出错
 */
int VimStreamDecrypt(int ch, const unsigned char* srcData, int len, int flag, unsigned char* dstData);

/**
 * @brief 关闭已打开的解密通道
 * @param ch 输入参数，指定要关闭的解密通道号
 * @note
 * @return 无
 */
void VimStreamDecryptChClose(int ch);


/******************jpeg编码相关接口**********************/
/**
 * @brief 启动jpeg编码通道
 * @param chId 输入参数，用户指定编码通道号，范围：0~7
 * @param vInfo 输入参数，指定编码通道的视频输入参数
 * @param level 输入参数，指定编码级别，级别越高编码后的jpeg图片质量越好，范围:1~6
 * @param cb 输入参数，编码后的jpeg图片数据回调
 * @param userData 输入参数，用户数据
 * @note
 * @return 成功：0，错误码：1：参数出错误、2：创建通道失败
 */
int VimJpegEncChOpen(int chId, const VFrameInfo* vInfo, int level, JpegPacketCb cb, void* userData);

/**
 * @brief 向编码通道推送一帧YUV数据
 * @param chId 输入参数，用户指定编码器通道号
 * @param data 输入参数，YUV数据内存地址
 * @param len 输入参数，YUV数据长度
 * @param pts 输入参数，视频帧时间戳，单位为毫秒
 * @note 接口内部copy了帧数据
 * @return 成功：0，错误码：-1：参数错误、1：内部队列已满、2：通道没有启动
 */
int VimJpegEncChPushFrame(int chId, const char* data, int len , long long pts);

/**
 * @brief 停止jpeg编码通道
 * @param chId 输入参数，通道号
 * @note
 * @return 成功：0，错误码：1：通道不存在
 */
int VimJpegEncChClose(int chId);


/******************解码相关接口**********************/
/**
 * @brief 创建解码通道
 * @param fmt 输入参数，视频编码格式
 * @param vo 输入参数，视频图像输出的vo或者vpss模块信息，调用接口前用户需要先创建好对应的模块通道
 * @note 
 * @return 成功：返回>=0的通道号，错误码：-1：参数错误，-2:创建解码通道失败
 */
int VimVdecChStart(AV_CODEC_FMT fmt, const VoModInfo* vo, AV_FRAME_FMT destFmt);

/**
 * @brief 向以创建的解码通道推送一帧待解码帧数据
 * @param ch 输入参数，用户指定通道号
 * @param fmt 输入参数，帧数据类型
 * @param data 输入参数，帧数据内存地址
 * @param len 输入参数，帧数据长度
 * @param pts 输入参数，视频帧显示时间戳，单位为毫秒
 * @param dts 输入参数，视频帧编码时间戳，单位为毫秒
 * @param flag 输入参数，帧类型标志，0：为非关键帧，其它值为关键帧
 * @note
 * @return 成功：0，错误码：1：解码器超时、2：没找到通道、3：其它错误
 */
int VimVdecChPushData(int ch, AV_CODEC_FMT fmt, const char* data, int len, long long pts, long long dts, int flag);

/**
 * @brief 查询解码器是否已经解码出帧
 * @param ch 输入参数，指定通道号
 * @param w 输出参数，成功返回视频的宽
 * @param h 输出参数，成功返回视频的高
 * @note 
 * @return 成功：返回0，错误码：1：没有找到通道，2:没有解码成功帧
 */
int VimVdecChGetHeader(int ch, int* w, int* h);

/**
 * @brief 关闭通道
 * @param ch 输入参数，指定通道号
 * @note 
 * @return 成功：返回0，错误码：1：没找到通道
 */
int VimVdecChStop(int ch);

#ifdef __cplusplus
}
#endif

#endif