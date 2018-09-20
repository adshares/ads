#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <string>
#include <fstream>
#include <algorithm>
#include <time.h>
#include <mutex>

#include "helper/blocks.h"

namespace logging {

static struct
{
    std::recursive_mutex file_mtx;
    FILE *log_file;
    LoggingLevel level;
    LoggingSource source;
} settings;


static const char *level_names[] =
{
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

void update_log_level(const char* iniFilePath)
{
    const std::string KEYWORD = "log_level";
    const int length = KEYWORD.length();

    std::ifstream ini_file(iniFilePath);
    std::string line;
    while(std::getline(ini_file, line))
    {
        std::size_t pos = line.find(KEYWORD);
        if (pos != std::string::npos)
        {
            line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
            if (line[0] != ';' && pos == 0)
            {
                try {
                    int log_level = std::stoi(line.substr(length+1));
                    settings.level = (LoggingLevel)log_level;
                } catch (std::exception) {}
                break;
            }
        }
    }
}

static void lock(void)
{
    settings.file_mtx.lock();
}


static void unlock(void)
{
    settings.file_mtx.unlock();
}

void set_level(LoggingLevel level)
{
    settings.level = level;
}

void set_log_file(FILE* file)
{
    settings.log_file = file;
}

void set_log_source(LoggingSource source)
{
    settings.source = source;
}

void log_log(LoggingLevel level, const char* function, int line, const char *fmt, ...)
{
    if (level < settings.level) {
        return;
    }

    lock();

    time_t t = time(NULL);
    va_list args;

    if (settings.source & LoggingSource::LOG_CONSOLE)
    {
        fprintf(stderr, "[%ld] [%s:%d] %-5s: ", t, function, line, level_names[level]);
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fflush(stderr);
    }

    if (settings.source & LoggingSource::LOG_FILE)
    {
        fprintf(settings.log_file, "[%ld] [%s:%d] %-5s: ", t, function, line, level_names[level]);
        va_start(args, fmt);
        vfprintf(settings.log_file, fmt, args);
        va_end(args);
        fflush(settings.log_file);
    }

    unlock();
}

void change_log_file(uint32_t timestamp)
{
    lock();

    char filename[32];;
    uint32_t ntime=time(NULL);
    Helper::FileName::getName(filename, timestamp, "log.txt");

    fprintf(settings.log_file,"END: %08X\n",ntime);
    fclose(settings.log_file);

    settings.log_file = fopen(filename, "a");
    fprintf(settings.log_file,"START: %08X\n",ntime);

    update_log_level("options.cfg");

    unlock();
}

}
