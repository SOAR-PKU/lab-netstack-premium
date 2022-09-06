#ifndef __LOG_H__
#define __LOG_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "lock.h"

// set them use gcc -D option.
#ifndef PROGNAME
#define PROGNAME ""
#endif
#ifndef VERSION
#define VERSION ""
#endif

extern int verbose;
extern int logFD;
//extern FILE *logFile;
extern lock_t logLock;
extern sigset_t logMask, logOldMask;

#define LOGBUF_LEVEL 4
#define LOGBUF_LEN 1024
extern lock_t logNum;
extern char logBuf[1 << LOGBUF_LEVEL][LOGBUF_LEN << 1];

void setVerbose(int level);

void initLog();
void resetLogFile();
double getTimestamp();
void printInitLog();

static inline char* getBuf()
{
    return logBuf[atomicInc(&logNum) & ((1 << LOGBUF_LEVEL) - 1)];
}

// sigprocmask() is a virtual syscall on x64, so using it here will not cause
// performance issues.
/*
#define doLog(prefix, ...)                                          \
    {                                                               \
        lock(&logLock);                                             \
        fprintf(logFile, "%s(%14.6lf): ", prefix,  getTimestamp()); \
        fprintf(logFile, __VA_ARGS__);                              \
        fprintf(logFile, "\n");                                     \
        fflush(logFile);                                            \
        release(&logLock);                                          \
    }
*/

// A NEW doLog macro without locks
#define doLog(prefix, ...)                                          \
    {                                                               \
        sigprocmask(SIG_SETMASK, &logMask, &logOldMask);            \
        char *buf = getBuf();                                       \
        sprintf(buf + LOGBUF_LEN, __VA_ARGS__);                     \
        sprintf(buf, "%s(%14.6lf): %s\n", prefix,                   \
            getTimestamp(), buf + LOGBUF_LEN);                      \
        if (write(logFD, buf, strlen(buf)));                        \
        sigprocmask(SIG_SETMASK, &logOldMask, &logMask);            \
    }

#define logVerboseL(level, ...)                                         \
    do                                                                  \
    {                                                                   \
        if (verbose >= level)                                           \
        {                                                               \
            doLog("[ Verbose ]", __VA_ARGS__);                          \
        }                                                               \
    } while (0)

#define logVerbose(...) logVerboseL(1, __VA_ARGS__)

#define logMessage(...)                                             \
    do                                                              \
    {                                                               \
        doLog("[ Message ]", __VA_ARGS__);                          \
    } while (0)

#define logWarning(...)                                             \
    do                                                              \
    {                                                               \
        doLog("[ Warning ]", __VA_ARGS__);                          \
    } while (0)

#define logError(...)                                               \
    do                                                              \
    {                                                               \
        doLog("[  Error  ]", __VA_ARGS__);                          \
    } while (0)

#define logFatal(...)                                               \
    do                                                              \
    {                                                               \
        doLog("[  FATAL  ]", __VA_ARGS__);                          \
        exit(1);                                                    \
    } while (0)

#endif