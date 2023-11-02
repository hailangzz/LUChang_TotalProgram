#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "Logger.h"
#include <iomanip>
#include <sstream>
#include <stdio.h>
#include <stdarg.h>

#define LOGD printf
#define LOGE printf

namespace LOG {

using namespace std::chrono;

static size_t MAX_BUFFER_SIZE = 1 * 1024 * 1024;
static size_t MAX_LOG_ITEMS = 1000;

std::mutex Logger::mMtx;
bool Logger::mEnable = true;
Logger* Logger::mInstance = nullptr;

Logger::Logger()
    : mQueue(MAX_LOG_ITEMS)
    , mBuf(128)
{
}

Logger::~Logger()
{
}

std::string Logger::timePointToString(log_clock::time_point tp)
{
    auto ms = tp.time_since_epoch();
    auto const msecs = duration_cast<milliseconds>(ms).count() % 1000;
    std::time_t t = system_clock::to_time_t(tp);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "[%m-%d %T") << "." << msecs << "]";
    return ss.str();
}

bool Logger::getLogItem(log_msg &log)
{
    std::lock_guard<std::mutex> lock(mMtx);
    Logger* logger = getInstance();

    return logger->mQueue.dequeue_for(log, std::chrono::milliseconds(0));
}

void Logger::log(level::level_enum lvl, const std::string tag, const char* fmt, ...)
{
    std::lock_guard<std::mutex> lock(mMtx);
    Logger* logger = getInstance();
    va_list aptr;
    int ret;

    va_start(aptr, fmt);
    while ((ret = vsnprintf(logger->mBuf.data(), logger->mBuf.size(), fmt, aptr)) < 0) {
        if (logger->mBuf.size() * 2 < MAX_BUFFER_SIZE) {
            logger->mBuf.resize(logger->mBuf.size() * 2);
        }
    }
    va_end(aptr);

    if (ret < 0) {
        LOGD("[error] log is too long\n");
        return;
    }

    if (ret == static_cast<int>(logger->mBuf.size())) {
        logger->mBuf.data()[logger->mBuf.size() - 1] = '\0';
    }

    log_msg log(tag, lvl, std::string(logger->mBuf.data()));
    logger->mQueue.enqueue_nowait(std::move(log));
}

}
