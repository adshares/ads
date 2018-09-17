#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <boost/current_function.hpp>

#ifndef __LINE__
#define __LINE__ 0
#endif /*__LINE__*/

#define TLOG(...) logging::log_log(logging::LOG_TRACE, BOOST_CURRENT_FUNCTION, __LINE__, __VA_ARGS__)
#define DLOG(...) logging::log_log(logging::LOG_DEBUG, BOOST_CURRENT_FUNCTION, __LINE__, __VA_ARGS__)
#define ILOG(...) logging::log_log(logging::LOG_INFO,  BOOST_CURRENT_FUNCTION, __LINE__, __VA_ARGS__)
#define WLOG(...) logging::log_log(logging::LOG_WARN,  BOOST_CURRENT_FUNCTION, __LINE__, __VA_ARGS__)
#define ELOG(...) logging::log_log(logging::LOG_ERROR, BOOST_CURRENT_FUNCTION, __LINE__, __VA_ARGS__)
#define FLOG(...) logging::log_log(logging::LOG_FATAL, BOOST_CURRENT_FUNCTION, __LINE__, __VA_ARGS__)

namespace logging {

enum LoggingLevel {
    LOG_TRACE = 0,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

enum LoggingSource {
    LOG_NONE = 0,
    LOG_CONSOLE,
    LOG_FILE,
    LOG_ALL
};

void set_level(LoggingLevel level);
void set_log_source(LoggingSource source);
void change_log_file(uint32_t timestamp);
void set_log_file(FILE* file);
void log_log(LoggingLevel level, const char *function, int line, const char *fmt, ...);

}

#endif // LOGGING_H
