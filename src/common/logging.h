#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

enum {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

#define TLOG(...) logging::log_log(LOG_TRACE, __VA_ARGS__)
#define DLOG(...) logging::log_log(LOG_DEBUG, __VA_ARGS__)
#define ILOG(...) logging::log_log(LOG_INFO,  __VA_ARGS__)
#define WLOG(...) logging::log_log(LOG_WARN,  __VA_ARGS__)
#define ELOG(...) logging::log_log(LOG_ERROR, __VA_ARGS__)
#define FLOG(...) logging::log_log(LOG_FATAL, __VA_ARGS__)

namespace logging {

void set_level(int level);
void change_log_file(uint32_t timestamp);
void set_log_file(FILE* file);
void log_log(int level, const char *fmt, ...);

}

#endif // LOGGING_H
