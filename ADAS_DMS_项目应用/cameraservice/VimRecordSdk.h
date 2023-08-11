#ifndef _VIMRECORDSDK_H_
#define _VIMRECORDSDK_H_
#include"VimMediaType.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief vimicro sysapi init
 * @note �˽ӿ�Ҳ�����Լ�ʹ��vimicro sys apiʵ�֣������нӿڵ���ǰʹ��
 */
int VIMSysInit(void);

/**
 * @brief vimicro sysapi exit
 *
 */
void VIMSysExit(void);

/******************��Ƶ����ģ����ؽӿ�**********************/
/**
 * @brief ������Ƶ����ͨ��
 * @param chId ����������û�ָ������ͨ���ţ���Χ��MIN_VENC_CH~MAX_VENC_CH
 * @param fmt ���������ָ���������������ͣ�Ŀǰ��֧��AV_FMT_H265��AV_FMT_SVAC2
 * @param vInfo ���������ָ������ͨ������Ƶ�������
 * @param bitRate ���������ָ����������
 * @param type ����������û�ָ������ͨ��Դ������Ϊ��RECORD_SOURCE���鿴VimMediaType.h��
 * @param srcMod �����������typeΪBIND_VPSS_TYPEʱ��ָ��vpss��ͨ����Ϣ��sdk�ڲ��󶨣����ΪNULL����Ҫ�û��Լ���
 * @param useVencBuf ����������Ƿ���Ҫ����ͨ����������֡���棬
 * ��typeΪPUSH_FRAME_TYPEʱ�̶�Ϊ1������Ҫ���ݰ�����ģ��vpss�����ͨ������u32VoIsVpssBuf����
 * @note ���ɵ�¼���ļ�����ʽ�磺ch0_20220101_000446998.mp4��ch0���Ǵ���ͨ��0�����ӿڵĵ�һ��������
 * @return �ɹ���0�������룺1��û�г�ʼ����2��ͨ���Ŵ���3��ͨ���Ѿ�������4������¼���߳�ʧ�ܡ�5��������Ƶ������ʧ��
 */
int VimVencChStart(int chId, AV_CODEC_FMT fmt, const VFrameInfo* vInfo, int bitRate, int type, const VimModule* srcMod, int useVencBuf);

/**
 * @brief �����ͨ������һ֡YUV����
 * @param chId ����������û�ָ��������ͨ����
 * @param data ���������YUV�����ڴ��ַ
 * @param len ���������YUV���ݳ���
 * @param pts �����������Ƶ֡ʱ�������λΪ����
 * @param copy ����������Ƿ񿽱�yuv���ݣ�Ϊ0ʱ����ʾ��������data�ڴ��ַ����Ϊmalloc�������룬����ֻ�е��ӿڷ���ʧ���û�����Ҫfree�ͷŴ��ڴ棬������sdk�ڲ��ͷ�
 * @note ֻ�е�VimVencChStart��typeΪPUSH_FRAME_TYPE����ʱ����Ҫʹ�ô˽ӿ�
 * @return �ɹ���0�������룺-1����������1���ڲ�����������2��ͨ��û������
 */
int VimVencChPushVFrame(int chId, const char* data, int len , long long pts, int copy);

/**
 * @brief �����Ƶ���������֡���ݻص�
 * @param chId ������ͨ����
 * @param cb �ص��ӿ�
 * @param userData �û�����
 * @note
 * @return �ɹ���0�������룺1��ͨ��������
 */
int VimVencChAddConsumer(int chId, VencPacketCb cb, void* userData);

/**
 * @brief ɾ������������ص�
 * @param ch ���������ͨ����
 * @param cb �ص��ӿ�
 * @note
 * @return �ɹ���0�������룺1��ͨ��������
 */
int VimVencChDelConsumer(int chId, VencPacketCb cb);

/**
 * @brief ֹͣ��Ƶ����ͨ��
 * @param ch ���������ͨ����
 * @note
 * @return �ɹ���0�������룺1��ͨ��������
 */
int VimVencChStop(int chId);


/******************��Ƶת��ģ����ؽӿ�**********************/
/**
 * @brief ��һ����Ƶת����
 * @param srcFmt ���������Դ�����ʽ
 * @param dstFmt ���������Ŀ������ʽ
 * @param cb �������������ص��ӿ�
 * @param userData ����������û�����
 * @note Ŀǰ��֧��SVAC2תH265����ഴ��4��������
 * @return �ɹ����أ�����MAX_VENC_CH��ͨ����(Ϊ��������������ص��ӿ�)�������룺-1����������-2���Ѵﵽ���ͨ����
 */
int VimTranscodeOpen(AV_CODEC_FMT srcFmt, AV_CODEC_FMT dstFmt, int bitRate, VencPacketCb cb, void* userData);

/**
 * @brief ��ת��ͨ������һ֡�ѱ�������
 * @param ch ����������û�ָ��ת��ͨ����
 * @param fmt �������������֡��������
 * @param data ����������ѱ���֡�����ڴ��ַ
 * @param len ���������֡���ݳ���
 * @param pts �����������Ƶ֡��ʾʱ�������λΪ����
 * @param dts �����������Ƶ֡����ʱ�������λΪ����
 * @param flag ���������֡���ͱ�־��0��Ϊ�ǹؼ�֡������ֵΪ�ؼ�֡
 * @note
 * @return �ɹ���0�������룺1����������2��û�ж�Ӧͨ����3��ת�����������4��ת�����
 */
int VimTranscodePushPacketData(int ch, AV_CODEC_FMT fmt, const unsigned char* data, int len , long long pts, long long dts, int flag);

/**
 * @brief ֹͣת��ͨ��
 * @param ch ���������ͨ����
 * @note
 * @return �ɹ���0�������룺1��ͨ��������
 */
int VimTranscodeClose(int ch);


/******************¼��ģ����ؽӿ�**********************
¼��ģ��֧���ڲ�ֱ�Ӱ�����ı���ģ�飬ֹͣʱ�Զ����
Ҳ���Բ�������ı���ģ�飬�Լ�ʵ�ֱ���Ȼ��ʹ��VimRecordChPushPacketData�����ѱ�������¼��
*/

/**
 * @brief ¼��sdk��ʼ�ӿ�
 * @param recordDir ָ��¼���ļ����·����·�����������ʣ��ռ����200M������ҪSDK�ڲ��Զ�����¼���ļ���������ʱ��Ҫָ���˲���
 * @param cb ��̬��ȡ¼���ļ�ȫ·���ص����������û���Ҫ�Լ�����¼���ļ�ʱ��Ҫʵ�ִ˺�������ʱ�����recordDir����
 * @param handler ¼��ͻط�֧���ⲿ��д�ӿ�
 * @param initHawk �Ƿ�����hawksdk��0:��ʶ������������ֵΪ�����������Ҫsvac�ӽ������������
 * @param pwdMd5Str ���������¼�Ƽ���¼��ʱ��Ҫ�ṩ¼�������md5�����ַ�����Ŀǰ��֧����Ƶ��svac2�ļ��ܣ�������info->vFmtΪAV_FMT_SVAC2��info->vEncConfigΪ��0ʱ�������ṩ�β���
 * @param param ���������vissƽ̨GB28181Э���������������Ҫ����vissƽ̨��Ҫ�˲�������GB28181�������ã�����initHawk���뿪��
 * @note ����������¼��ӿڵ���ǰʹ�ã�ֻ��Ҫ����һ��
 * @return �ɹ���0�������룺1���Ѿ���ʼ����2����Ч�Ĳ�����3��·���ռ䲻�㡢4�������̳߳���5������hawk����
 */
int VimRecordInit(const char* recordDir, GetRecFullPathCb cb, ExtRwHanlder* handler, int initHawk, const char* pwdMd5Str, const VissGbParam* param);

/**
 * @brief ����¼��ͨ��
 * @param ch ����������û�ָ��¼��ͨ���ţ���Χ��MIN_REC_CH~MAX_REC_CH
 * @param bindVencCh ������������ڵ���0ʱ�ڲ�����Ƶ����ͨ���ţ�С��0��Ҫʹ��VimRecordChPushPacketData������Ƶ֡¼��
 * @param info ���������ָ��¼��ͨ��ý�����
 * @note ���ɵ�¼���ļ�����ʽ�磺ch0_20220101_000446998.mp4��ch0���Ǵ���ͨ��0�����ӿڵĵ�һ��������
 * @return �ɹ���0�������룺1��û�г�ʼ����2��ͨ���Ŵ���3��ͨ���Ѿ�������4:¼���������5������¼���߳�ʧ�ܡ�6:��������ͨ��ʧ�ܣ�7������Ƶ������ʧ��
 */
int VimRecordChStart(int ch, int bindVencCh, const MediaInfo* info);

/**
 * @brief ��¼��ͨ������һ֡�ѱ�������
 * @param ch ����������û�ָ��¼��ͨ����
 * @param fmt �������������֡��������
 * @param data ����������ѱ���֡�����ڴ��ַ
 * @param len ���������֡���ݳ���
 * @param pts �����������Ƶ֡��ʾʱ�������λΪ����
 * @param dts �����������Ƶ֡����ʱ�������λΪ����
 * @param flag ���������֡���ͱ�־��0��Ϊ�ǹؼ�֡������ֵΪ�ؼ�֡
 * @note �ӿ��ڲ�copy��֡���ݣ�ֻ�е�VimRecordChStart��bindVencChС��0ʱ����Ҫ�νӿ���������Ƶ֡¼��
 * @return �ɹ���0�������룺-1����������1���ڲ�����������2��ͨ��û������
 */
int VimRecordChPushPacketData(int ch, AV_CODEC_FMT fmt, const unsigned char* data, int len, long long pts, long long dts, int flag);

/**
 * @brief ֹͣ¼��ͨ��
 * @param ch ����������û�ָ��¼��ͨ����
 * @note
 * @return �ɹ���0�������룺1��û�г�ʼ����2��ͨ��������
 */
int VimRecordChStop(int ch);

/**
 * @brief ����ͨ���¼�¼�񣬹̶�¼��ǰ��30s
 * @param ch ����������û�ָ��¼��ͨ���ţ�������һ���Ѿ�������ʱ¼���ͨ��
 * @param event ����������û�ָ��¼���¼����ͣ��������0
 * @note
 * @return �ɹ���0�������룺1:û�г�ʼ����2��ͨ�������ڡ�3��ͨ��û������4����ͨ�����ڽ����¼�¼��5�������¼�¼�����6��δ֪����
 */
int VimStartEventRecord(int ch, int event);

/**
 * @brief ¼��sdk����ʼ��
 * @note ������ֹͣ����¼��ͨ�������ܵ��ô˽ӿ�
 * @return �ɹ���0�������룺1��¼��ͨ��û��ֹͣ
 */
int VimRecordUnInit(void);

/**
 * @brief �����豸��Ƶ���ݻص��ӿ�
 * @param cb �����������Ƶ�ص�����
 * @param userData ����������û����ݵ�ַ������ΪNULL
 * @note
 * @return �ɹ���0�������룺��
 */
int VimSetDeviceAudioCb(AudioFrameCb cb, void* userData);

/******************¼��ط���ؽӿ�**********************/

/**
 * @brief �������������ļ�����ȡ¼���ļ���Ϣ
 * @param file �����������Ҫ���ŵ�¼���ļ�������·��
 * @param info ����������ӿڵ��óɹ�ʱ����¼����Ϣ
 * @note ����16������ͨ��
 * @return �ɹ�������0�Ĳ��ž���������룺0:����ͨ���ѵ����ޣ�-1�����ļ�ʧ�ܣ�-2���ļ���ʽ��֧�ֲ���
 */
int VimPlayerOpen(const char* file, MediaInfo* info);

/**
 * @brief ����¼���ļ�����
 * @param handle ������������ž��
 * @param vo �����������Ƶͼ�������vo����vpssģ����Ϣ�����ýӿ�ǰ�û���Ҫ�ȴ����ö�Ӧ��ģ��ͨ��
 * @param cb �������������״̬�¼��ص������������Ž��ȡ����Ž��������ų���
 * @param pwdMd5Str ������������ż�����Ƶ��ʱ��Ҫ�ṩ�������룬ֵΪ��������ͨ��md5���ܺ�32���ֽڵ�Сд�ַ���������ļ�Ϊ�Ǽ��ܵ�����NULL����
 * @param userData ����������û�����
 * @note 
 * @return �ɹ���0�������룺1������ͨ��û�򿪣�2:�������3����������ͨ��ʧ�ܣ�4����������������5�����������̳߳���
 */
int VimPlayerStart(int handle, const VoModInfo* vo, PlayCallBack cb, const char* pwdMd5Str, void* userData);

/**
 * @brief ���ſ��ƽӿ�
 * @param handle ������������ž��
 * @param cmd ������������ſ�������
 * cmd��PLAY_PAUSE_CMD��PLAY_START_CMD��data���������壬����ΪNULL��
 * cmd: PLAY_SEEK_CMDʱ��dataΪdouble����ָ�룬ָ����ת��¼��ָ��ʱ��㣬��λΪ��
 * cmd: PLAY_SPEED_CMDʱ��dataΪint����ָ�룬�������û���ȡ�����ٲ��ţ�0Ϊȡ�����ٲ��ţ�����ֵΪ���ٲ���
 * @param data ������������������������ͬ��cmd��Ӧ��ͬ�Ĳ�������:
 * @note
 * @return �ɹ���0�������룺1������ͨ�������ڣ�2���������
 */
int VimPlayerCtrl(int handle, int cmd, void* data);

/**
 * @brief �رղ���
 * @param handle ������������ž��
 * @note
 * @return �ɹ���0�������룺1������ͨ��������
 */
int VimPlayerClose(int handle);

/**
 * @brief �ַ���md5���ܽӿ�
 * @param inStr �����������Ҫ���ܵ��ַ���
 * @param outMd5Str ������������md5���ܺ���ַ�����Ϊ32���ֽڵ�Сд�ַ����û��ṩ���ڵ���33���ֽڵ��ڴ��ַ
 * @note
 * @return ��
 */
void StringMd5Sum(const char* inStr, char* outMd5Str);

/**
 * @brief ����rtsp������
 * @param port ����������������˿�
 * @param handler ���������rtsp�¼��ص��ṹ��
 * @note �ͻ���rtsp�������Ӹ�ʽ:rtsp://ip:port?param1&param2
?* param1:��ѡ���ļ�����·����base64����
?* param2:��ѡ�������ļ�����Կ������ǷǼ����ļ�������Ҫ��
 * @return �ɹ���0�������룺1������ʧ��
 */
int VimRtspStart(int port, RtspEventHandler handler);

/**
 * @brief ��¼���ļ��������⸴��������ȡý����Ϣ
 * @param file ����������ļ�ȫ·��
 * @param pHandle �������������һ��demuxer���
 * @param info ������������ý����Ϣ
 * @note
 * @return �ɹ���0�������룺1����������2��ý���ļ���ʽ����3��ý���ʽ��֧��
 */
int VimMpsDemuxerOpenFile(const char *file, MPSHANDLE *pHandle, MediaInfo *info);

/**
 * @brief �Ӵ򿪵�¼���ļ��л�ȡ����Ƶ������ص�vkek��vkekver���ں���Ľ�����Ƶ֡����
 * @param h ���������demuxer���
 * @param pwdMd5Str ���������¼���ļ����������md5��
 * @param vkek ������������16�ֽڳ��ȵļ���key
 * @param vkekver ����������������Ϊ20�ֽڵ��ַ���key version
 * @note ���ż���¼���ļ�ʱ����Ҫʹ�ô˽ӿ�
 * @return �ɹ���0�������룺1����������2��ý���ļ���ʽ����3����������������
 */
int VimMpsDemuxerGetVkek(MPSHANDLE h, const char* pwdMd5Str, char* vkek, char* vkekver);

/**
 * @brief ��ȡý�����ݰ�
 * @param h ���������demuxer���
 * @param packet ��������� �ɹ�����ý������
 * @note
 * @return �ɹ�������0�������룺0���ļ�β��-1����������
 */
int VimMpsDemuxerReadPacket(MPSHANDLE h, MpsPacket *packet);

/**
 * @brief �ͷŵ�VimMpsDemuxerReadPacket��ȡ�����ݰ�
 * @param h ���������demuxer���
 * @param packet ��������� ͨ��VimMpsDemuxerReadPacket��ȡ������
 * @note
 * @return ��
 */
void VimMpsDemuxerReleasePacket(MPSHANDLE h, MpsPacket *packet);

/**
 * @brief �ļ�λ����ת,��ָ����λ��
 * @param h ���������demuxer���
 * @param pos ���������ָ����תλ�ã���λΪ��
 * @note
 * @return �ɹ���0�������룺1����Ч�Ĳ�����2��seekλ��pos��Ч
 */
int VimMpsDemuxerSeek(MPSHANDLE h, double pos);

/**
 * @brief �ر��Ѵ򿪵Ľ⸴����
 * @note
 * @return ��
 */
void VimMpsDemuxerClose(MPSHANDLE h);


/******************��������ؽӿ�**********************/
/*Ŀǰֻ��SVAC2������������*/

/**
 * @brief ��һ������ͨ��
 * @param vkek �������������16�ֽڳ��ȵļ���key�������ɽӿ�VimMpsDemuxerGetVkek��ȡ
 * @param vkekver ���������key version
 * @note
 * @return �ɹ����ش��ڵ���0��ͨ���ţ������룺-1����������-2������ģ��δ������-3������ͨ������ʧ��
 */
int VimStreamDecryptChOpen(const char* vkek, const char* vkekver);

/**
 * @brief ����һ֡��Ƶ����
 * @param ch ���������ָ������ͨ���ţ�VimStreamDecryptChOpen����ֵ
 * @param srcData �������������ǰ���ݻ���
 * @param len �������������ǰ���ݳ���
 * @param flag �����������Ƶ֡���ͣ�0Ϊ�ǹؼ�֡������ֵΪ�ؼ�֡
 * @param dstData ������������ܺ���Ƶ֡����������棬���ڴ����û��Լ��������ҳ��ȱ���Ƚ���ǰ���ݳ��ȳ�256���ֽڣ�
 * @note ͬ�����ܽӿ�
 * @return �ɹ����ش���0�Ľ��ܺ����ݳ��ȣ������룺-1����������-2������ģ��δ������-3��δ�ҵ�����ͨ����-4�����ܳ���
 */
int VimStreamDecrypt(int ch, const unsigned char* srcData, int len, int flag, unsigned char* dstData);

/**
 * @brief �ر��Ѵ򿪵Ľ���ͨ��
 * @param ch ���������ָ��Ҫ�رյĽ���ͨ����
 * @note
 * @return ��
 */
void VimStreamDecryptChClose(int ch);


/******************jpeg������ؽӿ�**********************/
/**
 * @brief ����jpeg����ͨ��
 * @param chId ����������û�ָ������ͨ���ţ���Χ��0~7
 * @param vInfo ���������ָ������ͨ������Ƶ�������
 * @param level ���������ָ�����뼶�𣬼���Խ�߱�����jpegͼƬ����Խ�ã���Χ:1~6
 * @param cb ���������������jpegͼƬ���ݻص�
 * @param userData ����������û�����
 * @note
 * @return �ɹ���0�������룺1������������2������ͨ��ʧ��
 */
int VimJpegEncChOpen(int chId, const VFrameInfo* vInfo, int level, JpegPacketCb cb, void* userData);

/**
 * @brief �����ͨ������һ֡YUV����
 * @param chId ����������û�ָ��������ͨ����
 * @param data ���������YUV�����ڴ��ַ
 * @param len ���������YUV���ݳ���
 * @param pts �����������Ƶ֡ʱ�������λΪ����
 * @note �ӿ��ڲ�copy��֡����
 * @return �ɹ���0�������룺-1����������1���ڲ�����������2��ͨ��û������
 */
int VimJpegEncChPushFrame(int chId, const char* data, int len , long long pts);

/**
 * @brief ֹͣjpeg����ͨ��
 * @param chId ���������ͨ����
 * @note
 * @return �ɹ���0�������룺1��ͨ��������
 */
int VimJpegEncChClose(int chId);


/******************������ؽӿ�**********************/
/**
 * @brief ��������ͨ��
 * @param fmt �����������Ƶ�����ʽ
 * @param vo �����������Ƶͼ�������vo����vpssģ����Ϣ�����ýӿ�ǰ�û���Ҫ�ȴ����ö�Ӧ��ģ��ͨ��
 * @note 
 * @return �ɹ�������>=0��ͨ���ţ������룺-1����������-2:��������ͨ��ʧ��
 */
int VimVdecChStart(AV_CODEC_FMT fmt, const VoModInfo* vo, AV_FRAME_FMT destFmt);

/**
 * @brief ���Դ����Ľ���ͨ������һ֡������֡����
 * @param ch ����������û�ָ��ͨ����
 * @param fmt ���������֡��������
 * @param data ���������֡�����ڴ��ַ
 * @param len ���������֡���ݳ���
 * @param pts �����������Ƶ֡��ʾʱ�������λΪ����
 * @param dts �����������Ƶ֡����ʱ�������λΪ����
 * @param flag ���������֡���ͱ�־��0��Ϊ�ǹؼ�֡������ֵΪ�ؼ�֡
 * @note
 * @return �ɹ���0�������룺1����������ʱ��2��û�ҵ�ͨ����3����������
 */
int VimVdecChPushData(int ch, AV_CODEC_FMT fmt, const char* data, int len, long long pts, long long dts, int flag);

/**
 * @brief ��ѯ�������Ƿ��Ѿ������֡
 * @param ch ���������ָ��ͨ����
 * @param w ����������ɹ�������Ƶ�Ŀ�
 * @param h ����������ɹ�������Ƶ�ĸ�
 * @note 
 * @return �ɹ�������0�������룺1��û���ҵ�ͨ����2:û�н���ɹ�֡
 */
int VimVdecChGetHeader(int ch, int* w, int* h);

/**
 * @brief �ر�ͨ��
 * @param ch ���������ָ��ͨ����
 * @note 
 * @return �ɹ�������0�������룺1��û�ҵ�ͨ��
 */
int VimVdecChStop(int ch);

#ifdef __cplusplus
}
#endif

#endif