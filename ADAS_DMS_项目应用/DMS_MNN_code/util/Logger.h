#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <chrono>
#include <vector>
#include <mutex>
#include <sstream>
#include "mpmc_blocking_q.h"

#ifdef ANDROID
#include <android/log.h>
#endif

namespace LOG {

using log_clock = std::chrono::system_clock;

// Log level enum
namespace level {

enum level_enum
{
    trace,
    debug,
    info,
    warn,
    err,
    critical,
    off,
};
} // namespace level

struct log_msg
{
    log_msg() = default;
    log_msg(std::string logger_tag, level::level_enum lvl, std::string msg)
        : tag(logger_tag)
        , level(lvl)
        , time(std::chrono::system_clock::now())
        , payload(msg)
    {
    }
    
    log_msg& operator=(log_msg&& msg) noexcept {
        tag = std::move(msg.tag);
        level = msg.level;
        time = msg.time;
        payload = std::move(msg.payload);
        return *this;
    }

    std::string tag;
    level::level_enum level;
    log_clock::time_point time;
    std::string payload;
};

class Logger {
public:
    ~Logger();
    static Logger* getInstance() {
        if (mInstance == nullptr) {
            mInstance = new Logger();
        }
        return mInstance;
    }
    static std::string timePointToString(log_clock::time_point tp);
    static bool getLogItem(log_msg &log);
    static void log(level::level_enum lvl, const std::string tag, const char* fmt, ...);

    static void setEnable(bool enable) {
        // mEnable = enable;
        // if (!enable) {
        //     delete mInstance;
        //     mInstance = nullptr;
        // }
    }

    static bool isEnable() {
        return mEnable;
    }

private:
    Logger();

    mpmc_blocking_queue<log_msg> mQueue;
    std::vector<char> mBuf;
    static std::mutex mMtx;
    static bool mEnable;
    static Logger* mInstance;
};

template<typename... Args>
inline void trace(const std::string tag, const Args &... args)
{

#ifdef ANDROID
//    __android_log_print(ANDROID_LOG_VERBOSE, tag.c_str(), args...);
#else
    printf(args...);
#endif

    if (Logger::isEnable()) {
        Logger::log(LOG::level::trace, tag, args...);
    }
}

template<typename... Args>
inline void debug(const std::string tag, const Args &... args)
{

#ifdef ANDROID
//    __android_log_print(ANDROID_LOG_DEBUG, tag.c_str(), args...);
#else
    printf(args...);
#endif

    if (Logger::isEnable()) {
        Logger::log(LOG::level::debug, tag, args...);
    }
}

template<typename... Args>
inline void info(const std::string tag, const Args &... args)
{
#ifdef ANDROID
 //   __android_log_print(ANDROID_LOG_INFO, tag.c_str(), args...);
#else
    printf(args...);
#endif

    if (Logger::isEnable()) {
        Logger::log(LOG::level::info, tag, args...);
    }
}

template<typename... Args>
inline void warn(const std::string tag, const Args &... args)
{

#ifdef ANDROID
//    __android_log_print(ANDROID_LOG_WARN, tag.c_str(), args...);
#else
    printf(args...);
#endif

    if (Logger::isEnable()) {
        Logger::log(LOG::level::warn, tag, args...);
    }
}

template<typename... Args>
inline void error(const std::string tag, const Args &... args)
{

#ifdef ANDROID
//    __android_log_print(ANDROID_LOG_ERROR, tag.c_str(), args...);
#else
    printf(args...);
#endif

    if (Logger::isEnable()) {
        Logger::log(LOG::level::err, tag, args...);
    }
}

template<typename... Args>
inline void critical(const std::string tag, const Args &... args)
{

#ifdef ANDROID
//    __android_log_print(ANDROID_LOG_FATAL, tag.c_str(), args...);
#else
    printf(args...);
#endif

    if (Logger::isEnable()) {
        Logger::log(LOG::level::critical, tag, args...);
    }
}

} // namespace LOG

#endif //_LOGGER_H_
