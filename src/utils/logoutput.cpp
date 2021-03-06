/** Copyright (C) 2013 Ultimaker - Released under terms of the AGPLv3 License */
#include <stdio.h>
#include <stdarg.h>

#ifdef _OPENMP
    #include <omp.h>
#endif // _OPENMP
#include "logoutput.h"

#include "../Application.h"
#include "../communication/Communication.h"

namespace cura {

static int verbose_level;
static bool progressLogging;

void increaseVerboseLevel()
{
    verbose_level++;
}

void enableProgressLogging()
{
    progressLogging = true;
}

std::string buildMsg(const char* fmt, va_list args) {
    char buf[4096];
    vsnprintf(buf, 4096, fmt, args);
    return std::string(buf);
}

void logError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    std::string msg = buildMsg(fmt, args);

    #pragma omp critical
    {
        fprintf(stderr, "[ERROR] ");
        fprintf(stderr, "%s", msg.c_str());
        fflush(stderr);
    }
    va_end(args);

    if (Application::getInstance().communication) {
        Application::getInstance().communication->sendLogMessage(msg, 0);
    }
}

void logWarning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    std::string msg = buildMsg(fmt, args);

    #pragma omp critical
    {
        fprintf(stderr, "[WARNING] ");
        fprintf(stderr, "%s", msg.c_str());
        fflush(stderr);
    }
    va_end(args);

    if (Application::getInstance().communication) {
        Application::getInstance().communication->sendLogMessage(msg, 1);
    }
}

void logAlways(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    #pragma omp critical
    {
        vfprintf(stderr, fmt, args);
        fflush(stderr);
    }
    va_end(args);
}

void log(const char* fmt, ...)
{
    va_list args;
    if (verbose_level < 1)
        return;

    va_start(args, fmt);
    #pragma omp critical
    {
        vfprintf(stderr, fmt, args);
        fflush(stderr);
    }
    va_end(args);
}

void logDebug(const char* fmt, ...)
{
    va_list args;
    if (verbose_level < 2)
    {
        return;
    }
    va_start(args, fmt);
    #pragma omp critical
    {
        fprintf(stderr, "[DEBUG] ");
        vfprintf(stderr, fmt, args);
        fflush(stderr);
    }
    va_end(args);
}

void logProgress(const char* type, int value, int maxValue, float percent)
{
    if (!progressLogging)
        return;

    #pragma omp critical
    {
        fprintf(stderr, "Progress:%s:%i:%i \t%f%%\n", type, value, maxValue, percent);
        fflush(stderr);
    }
}

}//namespace cura
