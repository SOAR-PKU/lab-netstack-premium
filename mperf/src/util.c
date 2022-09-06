#include "util.h"

static char *smem[16];

sighandler_t signalNoRestart(int signum, sighandler_t handler) 
{
    struct sigaction action, oldAction;
    char errbuf[256];

    action.sa_handler = handler; 
    // block all signal(except SIGTERM) here. We use SIGTERM instead of SIGKILL
    // as a final way of closing a program.
    sigfillset(&action.sa_mask);
    sigdelset(&action.sa_mask, SIGTERM);
    action.sa_flags = 0;

    if (sigaction(signum, &action, &oldAction) < 0)
        logFatal("sigaction() failed(%s).", strerrorV(errno, errbuf));
    return oldAction.sa_handler;
}

// requires Linux Kernel >= 2.4!
void initSharedMem(int index)
{
    char errbuf[256];

    smem[index] = mmap(NULL, SHARED_BLOCK_LEN, PROT_READ | PROT_WRITE, 
        MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    if (smem[index] == MAP_FAILED)
    {
        logFatal("mmap() failed(%s).", strerrorV(errno, errbuf));
    }
    logVerbose("Shared memory block #%d(%lx) initialized.", index,
               (unsigned long)smem[index]);
}

void setMessage(int index, char *message)
{
    int len;

    len = strlen(message) + 1;
    *(int*)smem[index] = len;
    memcpy(smem[index] + sizeof(int), message, len);
}

int getMessage(int index, char *dest)
{
    int len;

    len = *(int*)smem[index];
    memcpy(dest, smem[index] + sizeof(int), len);
    return 0;
}

char *retstr(char ret, char *buf)
{
    switch (ret)
    {
    case RET_SUCC:
        sprintf(buf, "Success(RET_SUCC, %d)", RET_SUCC);
        break;
    case RET_EMSG:
        sprintf(buf, "Unrecognized control message(RET_EMSG, %d)",
            RET_EMSG);
        break;
    case RET_EKILL:
        sprintf(buf, "Controller failed to send signal(RET_EKILL, %d)",
            RET_EKILL);
        break;
    case RET_EREAD:
        sprintf(buf, "Failed to read message(RET_EREAD, %d)", 
            RET_EREAD);
        break;
    case RET_EWRITE:
        sprintf(buf, "Failed to write message(RET_EWRITE, %d)", 
            RET_EWRITE);
        break;
    case RET_EPROC:
        sprintf(buf, "Server not running(RET_EPROC, %d)", 
            RET_EPROC);
        break;
    default:
        sprintf(buf, "Value not defined(%d)", (int)ret);
    }
    return buf;
}

static void timeoutHandler(int sig)
{
    int be = errno;
    logError("Network timeout!");
    errno = be;
}

static inline void cancelTimeout(sighandler_t oldHandler)
{
    alarmWithLog(0);
    signalNoRestart(SIGALRM, oldHandler);
}

int rSendMessage(int connfd, const char *name, char *message, int len)
{
    static char buf[2048];
    // use this to make gcc happy...
    static int *ibuf = (int*)buf;
    sighandler_t oldHandler;
    char errbuf[256];

    *ibuf = len;
    memcpy(buf + sizeof(int), message, len);

    oldHandler = signalNoRestart(SIGALRM, timeoutHandler);
    alarmWithLog(MESSAGE_TIMEOUT);
    if (rio_writenr(connfd, buf, len + sizeof(int)) < len + (int)sizeof(int))
    {
        // we cancel the alarmWithLog first because log functions can cost a lot of
        // time when writing to _strange_ files...
        cancelTimeout(oldHandler);
        logError("Can't send message to %s!(%s)", name, 
            strerrorV(errno, errbuf));
        return RET_EWRITE;
    }
    else
    {
        cancelTimeout(oldHandler);
        logVerbose("Sent message with length=%d to %s", len, name);
        return RET_SUCC;
    }
}

int rReceiveMessage(int connfd, const char *name, char *buf)
{
    static const int len = sizeof(int);
    static int msglen;
    sighandler_t oldHandler;

    oldHandler = signalNoRestart(SIGALRM, timeoutHandler);
    alarmWithLog(MESSAGE_TIMEOUT);
    if (rio_readnr(connfd, (char*)&msglen, len) < len)
    {
        cancelTimeout(oldHandler);
        logError("Can't receive length from %s!", name);
        return RET_EREAD;
    }
    if (rio_readnr(connfd, buf, msglen) < msglen)
    {
        cancelTimeout(oldHandler);
        logError("Can't receive message from %s!", name);
        return RET_EREAD;
    }
    cancelTimeout(oldHandler);
    logVerbose("Received message with length=%d from %s", msglen, name);
    return RET_SUCC;
}

int rSendBytes(int connfd, const char *buf, int len, const char *errorText)
{
    sighandler_t oldHandler;
    char errbuf[256];

    oldHandler = signalNoRestart(SIGALRM, timeoutHandler);
    alarmWithLog(MESSAGE_TIMEOUT);
    if (rio_writenr(connfd, buf, len) < len)
    {
        cancelTimeout(oldHandler);
        logError("%s(%s)!", errorText, strerrorV(errno, errbuf));
        return RET_EWRITE;
    }
    cancelTimeout(oldHandler);
    logVerbose("Send complete.");
    return RET_SUCC;
}

int rRecvBytes(int connfd, char *buf, int len, const char *errorText)
{
    sighandler_t oldHandler;
    char errbuf[256];

    oldHandler = signalNoRestart(SIGALRM, timeoutHandler);
    alarmWithLog(MESSAGE_TIMEOUT);
    if (rio_readnr(connfd, buf, len) < len)
    {
        cancelTimeout(oldHandler);
        logError("%s(%s)!", errorText, strerrorV(errno, errbuf));
        return RET_EWRITE;
    }
    cancelTimeout(oldHandler);
    logVerbose("Receive complete.");
    return RET_SUCC;
}

// rio_read/write that returns on interrupt.
ssize_t rio_readnr(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = (char*)usrbuf;
    char errbuf[256];

    while (nleft > 0)
    {
        if ((nread = read(fd, bufp, nleft)) < 0)
        {
            if (errno == EINTR)
            {
                logVerbose("Read interrupted.");
                break;
            }
            else
            {
                logError("Read error(%s).", strerrorV(errno, errbuf));
                logVerboseL(2, "Got %ld bytes.", (ssize_t)(n - nleft));
                return -1; 
            }
        } 
        else if (nread == 0)
        {
            logMessage("EOF reached.");
            break;
        }    
        nleft -= nread;
        bufp += nread;
    }

    logVerboseL(2, "Got %ld bytes.", (ssize_t)(n - nleft));
    return (n - nleft);
}

ssize_t rio_writenr(int fd, const void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nwritten;
    const char *bufp = (const char*)usrbuf;
    char errbuf[256];

    while (nleft > 0) 
    {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) 
        {
            if (errno == EINTR)
            {
                logVerbose("Write interrupted.");
                n -= nleft;
                break;
            }
            else
            {
                logError("Write error(%s).", strerrorV(errno, errbuf));
                n = -1;
                break;
            }
        }
        nleft -= nwritten;
        bufp += nwritten;
    }

    logVerboseL(2, "Sent %ld bytes.", (ssize_t)n);
    return n;
}

// open_listenfd code comes from CS:APP2e example code pack: 
// http://csapp.cs.cmu.edu/public/code.html
// Modified to support bind local IP(code from below).
typedef struct sockaddr SA;
#define LISTENQ 1024 
int open_listenfd(const char *local, int port)
{
    int listenfd, optval = 1;
    int ret = -1;
    struct sockaddr_in *serveraddr, addr;
    struct addrinfo hints, *local_res;

    if (local) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = IPPROTO_TCP;
        if (getaddrinfo(local, NULL, &hints, &local_res) != 0)
            return -1;
    }

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        goto open_listenfd_out;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval, sizeof(int)) < 0)
        goto open_listenfd_out;

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    if (local)
    {
        serveraddr = (struct sockaddr_in*)local_res->ai_addr;
        serveraddr->sin_port = htons((unsigned short)port);
    }
    else
    {
        bzero((char *)&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons((unsigned short)port);
        serveraddr = &addr;
    }
    if (bind(listenfd, (SA *)serveraddr, sizeof(SA)) < 0)
        goto open_listenfd_out;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        goto open_listenfd_out;
    
    ret = listenfd;

open_listenfd_out:
    if (local)
    {
        freeaddrinfo(local_res);
    }
    return ret;
}

/* netdial and netannouce code comes from libtask: http://swtch.com/libtask/
 * Copyright: http://swtch.com/libtask/COPYRIGHT
*/

/* make connection to server */
int
netdial(int domain, int proto, char *local, int local_port, char *server, int port)
{
    struct addrinfo hints, *local_res, *server_res;
    int s;
    int val = 1;

    if (local) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = domain;
        hints.ai_socktype = proto;
        if (getaddrinfo(local, NULL, &hints, &local_res) != 0)
            return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = domain;
    hints.ai_socktype = proto;
    if (getaddrinfo(server, NULL, &hints, &server_res) != 0)
        return -1;

    s = socket(server_res->ai_family, proto, 0);
    if (s < 0) {
	if (local)
	    freeaddrinfo(local_res);
	freeaddrinfo(server_res);
        return -1;
    }

    if (local) {
        if (local_port) {
            struct sockaddr_in *lcladdr;
            lcladdr = (struct sockaddr_in *)local_res->ai_addr;
            lcladdr->sin_port = htons(local_port);
            local_res->ai_addr = (struct sockaddr *)lcladdr;
        }

        if (bind(s, (struct sockaddr *) local_res->ai_addr, local_res->ai_addrlen) < 0) {
	    close(s);
	    freeaddrinfo(local_res);
	    freeaddrinfo(server_res);
            return -1;
	}
        freeaddrinfo(local_res);
    }
    else if (local_port)
    {
        struct sockaddr_in myaddr;
        bzero(&myaddr, sizeof(struct sockaddr_in));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        myaddr.sin_port = htons(local_port);
        if (bind(s, (struct sockaddr*)&myaddr, 
            sizeof(struct sockaddr_in)) < 0) 
        {
            close(s);
            return -1;
        }
    }

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1) {
        close(s);
        return -1;
    }
    
    ((struct sockaddr_in *) server_res->ai_addr)->sin_port = htons(port);
    if (connect(s, (struct sockaddr *) server_res->ai_addr, server_res->ai_addrlen) < 0 && errno != EINPROGRESS) {
	close(s);
	freeaddrinfo(server_res);
        return -1;
    }

    freeaddrinfo(server_res);
    return s;
}
