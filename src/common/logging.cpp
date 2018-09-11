//void changeLogDir(uint32_t now);
//{
//    extern FILE* stdlog;
//    char filename[32];
//    Helper::FileName::getName(filename, now, "log.txt");
//    flog.lock();
//    fclose(stdlog);
//    stdlog=fopen(filename,"a");
//    uint32_t ntime=time(NULL);
//    fprintf(stdlog,"START: %08X\n",ntime);
//    flog.unlock();
//}

#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <mutex>

#include "helper/blocks.h"

namespace logging {

static struct
{
    std::mutex file_mtx;
    FILE *log_file;
    int level;
} settings;


//static const char *level_names[] =
//{
//  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
//};

static void lock(void)
{
    settings.file_mtx.lock();
}


static void unlock(void)
{
    settings.file_mtx.unlock();
}

void set_level(int level)
{
    settings.level = level;
}

void set_log_file(FILE* file)
{
    settings.log_file = file;
}

void log_log(int level, const char *fmt, ...)
{
    if (level < settings.level) {
    return;
    }

    /* Acquire lock */
    lock();

    /* Get current time */
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);

    va_list args;
    char buf[16];
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", lt)] = '\0';
//    fprintf(stderr, "%s %-5s: ", buf, level_names[level]);
    fprintf(stderr, "%s ", buf);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fflush(stderr);

    buf[strftime(buf, sizeof(buf), "%H:%M:%S", lt)] = '\0';
//    fprintf(settings.log_file, "%s %-5s: ", buf, level_names[level]);
    fprintf(settings.log_file, "%s ", buf);
    va_start(args, fmt);
    vfprintf(settings.log_file, fmt, args);
    va_end(args);
    fflush(settings.log_file);

    unlock();
}

void change_log_file(uint32_t timestamp)
{
    lock();

    char filename[32];;
    Helper::FileName::getName(filename, timestamp, "log.txt");
    fclose(settings.log_file);
    settings.log_file = fopen(filename, "a");
    uint32_t ntime=time(NULL);
    fprintf(settings.log_file,"START: %08X\n",ntime);

    unlock();
}

}
