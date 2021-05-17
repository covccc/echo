#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string>
#include <chrono>

#define MAX_LOG_TIME_BUF 64

// 获取当前时间，格式化成XXXX-XX-XX XX:XX:XX.XXX形式。主要提供给日志打印时使用
inline std::string ts_log() {
    auto now = std::chrono::system_clock::now();
    auto tm_t = std::chrono::system_clock::to_time_t(now);
    std::tm* lc = std::localtime(&tm_t);

    auto tm_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

    char tm_buf[MAX_LOG_TIME_BUF] = { 0 };
    snprintf(tm_buf, MAX_LOG_TIME_BUF, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
            lc->tm_year + 1900, lc->tm_mon + 1, lc->tm_mday, lc->tm_hour,
            lc->tm_min, lc->tm_sec, int(tm_ms % 1000));

    return std::string(tm_buf);
}

#define log_msg(fm, ...) \
    printf("[%s][%s:%s:%d] ", ts_log().c_str(), \
    __FILE__, __FUNCTION__, __LINE__);\
    printf(fm, ##__VA_ARGS__);\
    printf("\n");\
    fflush(stdout)

#endif