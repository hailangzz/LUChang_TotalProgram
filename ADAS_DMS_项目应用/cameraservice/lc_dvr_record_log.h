#ifndef LC_DVR_RECORD_LOG_H
#define LC_DVR_RECORD_LOG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
// place this include file in last of other include file in the c file.
/*
 * The default LOG_TAG is '_Road_rover', using these two lines to define newtag.
 * # undef LOG_TAG
 * #define LOG_TAG "newtag"
 */
#ifndef LOG_TAG
#define LOG_TAG "Mdvr_record"
#endif
enum {
    LC_LOG_LEVEL_ALL = 0,
    LC_LOG_LEVEL_DBG = 1,
    LC_LOG_LEVEL_INF = 2,
    LC_LOG_LEVEL_WRN = 3,
    LC_LOG_LEVEL_ERR = 4,
    LC_LOG_LEVEL_NON = 5,
};// log_level_t;

#ifndef LC_LOG_LEVEL
#define LC_LOG_LEVEL LC_LOG_LEVEL_ALL
#endif
/*
 * Using the following three macros for conveniently logging.
 */
//sf_log_printf -> printf
#ifndef LC_LOG_LEVEL
#define logd_lc(format,...)
#define logi_lc(format,...)
#define logw_lc(format,...)
#else
#define logd_lc(format,...)                                                      \
    do {                                                                         \
        if (LC_LOG_LEVEL <= LC_LOG_LEVEL_DBG) {                                   \
            printf("_%s (%c.line:%d): " format "\r\n",             \
						  LOG_TAG,                                               \
                          'd',                                                   \
                          __LINE__,                                              \
                          ##__VA_ARGS__);                                        \
        };                                                                       \
    } while (0)

#define logi_lc(format,...)                                                      \
    do {                                                                         \
        if (LC_LOG_LEVEL <= LC_LOG_LEVEL_INF) {                                   \
            printf("_%s (%c.line:%d): " format "\r\n",             \
                          LOG_TAG,                                               \
                          'i',                                                   \
                          __LINE__,                                              \
                          ##__VA_ARGS__);                                        \
        };                                                                       \
    } while (0)


#define logw_lc(format,...)                                                      \
    do {                                                                         \
        if (LC_LOG_LEVEL <= LC_LOG_LEVEL_WRN) {                                  \
            printf("_%s (%c.Line:%d) Wrn: " format "\r\n",     \
                          LOG_TAG,                                               \
                          'w',                                                   \
                          __LINE__,                                              \
                          ##__VA_ARGS__);                                        \
        }                                                                        \
    } while (0)
#endif

#define loge_lc(format, ...)                                                     \
    do {                                                                         \
            printf("xxxx(%s)xxxx: error @ %s(%d): " format "\r\n",               \
                          LOG_TAG,                                               \
                          __FUNCTION__,                                          \
                          __LINE__,                                              \
                          ##__VA_ARGS__);                                        \
    } while (0)



#ifdef __cplusplus
}
#endif

#endif /* __LC_LOG_H__ */

