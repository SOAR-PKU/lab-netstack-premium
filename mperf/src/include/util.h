#ifndef __UTIL_H__
#define __UTIL_H__

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include "lock.h"
#include "log.h"

#define PACKET_LEN 131072

#define SHARED_BLOCK_LEN 4096
#define SMEM_SERVERPID 0
#define SMEM_CONTROLLERPID 1
#define SMEM_MESSAGE 2

#define MESSAGE_TIMEOUT 10

#define SIG_TERM 0
#define SIG_CONF 1

#define RET_SUCC 0
#define RET_EMSG 1
#define RET_EKILL 2
#define RET_EREAD 3
#define RET_EWRITE 4
#define RET_EPROC 5

#define TYPE_LONG 0
#define TYPE_FIX 1
#define FLAG_REVERSE 2
#define TYPE_REVLONG (TYPE_LONG | FLAG_REVERSE)
#define TYPE_REVFIX (TYPE_FIX | FLAG_REVERSE)

#define BOOL(val) (!!(val))

typedef void (*sighandler_t)(int);

static inline char *strerrorV(int num, char *buf)
{
    sprintf(buf, "%d ", num);
    if (strerror_r(num, buf + strlen(buf), 128));
    return buf;
}

static inline unsigned int alarmWithLog(unsigned int seconds)
{
    logVerboseL(3, "Alarm %d", seconds);
    return alarm(seconds);
}

static inline void redirectLogTo(char *path)
{
    int newFD;
    char errbuf[256];
    
    // we don't mean to close the stdout/err files here.
    if (logFD != STDOUT_FILENO && logFD != STDERR_FILENO)
    {
        close(logFD);
    }
    if ((newFD = open(path, O_APPEND | O_WRONLY | O_CREAT, 0644)) == -1)
    {
        logFatal("Can't open log file %s(%s).", path, strerrorV(errno, errbuf));
    }
    else
    {
        logFD = newFD;
    }
}

extern const char *usage;
static inline void printUsageAndExit(char **argv)
{
    fprintf(stderr, "Usage: %s [options]\n%s\n", argv[0], usage);
    exit(0);
}

static inline void printVersionAndExit(const char *name)
{
    fprintf(stderr, "%s %s\n", name, VERSION);
    exit(0);
}

int netdial(int domain, int proto, char *local, int local_port,
    char *server, int port);
int open_listenfd(const char *local, int port);

void initSharedMem(int index);
void setMessage(int index, char *message);
int getMessage(int index, char *dest);
int rSendMessage(int connfd, const char *name, char *message, int len);
int rReceiveMessage(int connfd, const char *name, char *buf);
int rSendBytes(int connfd, const char *buf, int n, const char *errorText);
int rRecvBytes(int connfd, char *buf, int n, const char *errorText);

char *retstr(char ret, char *buf);

ssize_t rio_readnr(int fd, void *usrbuf, size_t n);
ssize_t rio_writenr(int fd, const void *usrbuf, size_t n);

sighandler_t signalNoRestart(int signum, sighandler_t handler);

static inline char rKill(int pid, const char *name, int sig)
{
    if (kill(pid, sig) < 0)
    {
        logError("Can't send signal to %s(%d)!", name, pid);
        return errno == EPERM ? RET_EPROC : RET_EKILL;
    }
    logVerbose("Signal %d sent to %s(%d).", sig, name, pid);
    return RET_SUCC;
}

static inline void failExit(const char *name)
{
    char errbuf[256];
    logFatal("%s() failed(%s).", name, strerrorV(errno, errbuf));
}

static inline int forceClose(int fd)
{
    int ret = close(fd);
    if (ret < 0)
    {
        failExit("close");
    }
    return ret;
}

static inline sighandler_t forceSignal(int signum, sighandler_t handler)
{
    sighandler_t ret = signal(signum, handler);
    if (ret == SIG_ERR)
    {
        failExit("signal");
    }
    return ret;
}

static inline int forceOpenListenFD(const char *local, int port)
{
    int ret = open_listenfd(local, port);
    if (ret < 0)
    {
        failExit("open_listenfd");
    }
    return ret;
}

#endif
