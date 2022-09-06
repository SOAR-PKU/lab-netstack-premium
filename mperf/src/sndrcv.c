#include "sndrcv.h"
#include "util.h"

lock_t sigalrm;
lock_t sigint;

void doLongTest(int connfd, int timelen, char *packetBuf)
{
    struct timeval st, ed;
    long sum = 0;
    int wrote = 0;
    double elapsed;
    char errbuf[256];

    logVerbose("Start long test.");

    alarmWithLog(timelen);

    gettimeofday(&st, NULL);

    while (continueTest())
    {
        int len;
#ifndef FIX
        len = PACKET_LEN;
        wrote = rio_writenr(connfd, packetBuf, len);
#else
        char c = 0;
        len = 1;
        wrote = rio_writenr(connfd, &c, len);
#endif
        if (wrote < len)
        {
            break;
        }
        sum += wrote;
#ifdef SPECIAL
        sleep(1);
#endif
#ifdef FIX
        usleep(50000);
#endif
    }

    gettimeofday(&ed, NULL);
    elapsed = (ed.tv_sec - st.tv_sec) + (ed.tv_usec - st.tv_usec) / 1000000.0;
    alarmWithLog(0);

    if (wrote < PACKET_LEN)
    {
        if (wrote < 0)
        {
            logError("Error occured when sending packets(%s)!", 
                strerrorV(errno, errbuf));
        }
        else if (!isLocked(&sigint))
        {
            logMessage("Long test terminated.");
        }
        else if (isLocked(&sigalrm))
        {
            logWarning("Send unexpectedly interrupted by signal.");
        }
    }

    logMessage("Long test summary:");
    logMessage("->Bytes transferred: %ld", sum);
    logMessage("->Time elapsed : %lfs", elapsed);
    logMessage("->Bandwidth: %lfBytes/sec", sum / elapsed);
}

#ifdef PROBE
void doProbe(int connfd, int sendInterval, int probeInterval, int size,
    int loop, char *packetBuf)
#else
void doFixTest(int connfd, int maxtime, int len, char *packetBuf)
#endif
{
#ifdef PROBE
    int maxtime = 0;
    int len = loop * size;
    sendInterval *= 1000;
    probeInterval *= 1000;
    int ind = 0;
    int lc = 0;
#endif
    int targ = len;
    struct timeval st, ed;
    double elapsed;
    int wrote = 0;
    int thislen = 0;
    char errbuf[256];

    logVerbose("Start fix test.");

    alarmWithLog(maxtime);
    
    // it's a virtual syscall on x64, so we assume it costs 
    // less than 1us.
    gettimeofday(&st, NULL);

    while (len > 0 && continueTest())
    {
#ifdef PROBE
        int thislen = 1;
#else
        int thislen = len > PACKET_LEN ? PACKET_LEN : len;
#endif
        int wrote = rio_writenr(connfd, packetBuf, thislen);

        if (wrote < thislen)
        {
            break;
        }

        len -= wrote;
#ifdef PROBE
        int sleeplen = len % size ? sendInterval : probeInterval;
        logVerboseL(2, "Probe packet %d:%d sent.", lc, ind);
        logVerboseL(2, "%d us before next probe packet...", sleeplen);
        if (++ind == size)
        {
            ind = 0;
            ++lc;
        }
        usleep(sleeplen);
#endif
    }

    gettimeofday(&ed, NULL);
    alarmWithLog(0);
    elapsed = (ed.tv_sec - st.tv_sec) + (ed.tv_usec - st.tv_usec) / 1000000.0;

    if (wrote < thislen)
    {
        if (wrote < 0)
        {
            logError("Error occured when sending packets(%s)!", 
                strerrorV(errno, errbuf));
        }
        else if (!isLocked(&sigint))
        {
            logMessage("Fix test terminated.");
        }
        else if (!isLocked(&sigalrm))
        {
            logWarning("Fix test timeout.");
        }
        else
        {
            logWarning("Send unexpectedly interrupted by signal.");
        }
        len -= wrote;
    }

    logMessage("Fix test summary:");
    logMessage("->Bytes to transfer: %d", targ);
    logMessage("->Bytes transferred: %d", targ - len);
    logMessage("->Bandwidth: %lfBytes/sec", (targ - len) / elapsed);
    logMessage("->Time elapsed : %lfs", elapsed);
}

void doReceive(int connfd, int timelen, char *recvBuf)
{
    int ret;
    char errbuf[256];
    double elapsed = 0;
    struct timeval st, ed;
    long byteReceived = 0;

//    setsockopt(connfd, SOL_SOCKET, SO_RCVBUF,
//        (const char*)&rwnd,sizeof(int));

    logVerbose("Start receving data.");
    logVerbose("Timeout threshold is %d", timelen);
    alarmWithLog(timelen);
    gettimeofday(&st, NULL);
    do
    {
#ifdef CHECK
        long i;
#endif
        errno = 0;
        if ((ret = rio_readnr(connfd, recvBuf, PACKET_LEN)) < PACKET_LEN)
        {
            if (ret < 0)
            {
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    logWarning("Connection broken(%s).", 
                        strerrorV(errno, errbuf));
                }
                else 
                {
                    logError("Unexpected read error(%s)!", 
                        strerrorV(errno, errbuf));
                }
            }
            else
            {
                if (errno == EINTR)
                {
                    if (!isLocked(&sigint))
                    {
                        logMessage("Ctrl+C received, terminate.");
                    }
                    else if (!isLocked(&sigalrm))
                    {
                        logMessage("Test timeout after %d seconds, terminate.",
                            timelen);
                    }
                    else
                    {
                        logError("Interrupted by unexpected signal!");
                    }
                }
                byteReceived += ret;
            }
            break;
        }

#ifdef CHECK
        for (i = 0; i < (PACKET_LEN >> 3); ++i)
        {
            if (((long*)recvBuf)[i] != i)
            {
                logFatal("Value error at %ld(got %ld, expected %ld)", 
                    byteReceived + (i << 3), ((long*)recvBuf)[i], i);
            }
        }
#endif
        byteReceived += ret;
    }
    while (continueTest());

    gettimeofday(&ed, NULL);
    alarmWithLog(0);
    
    elapsed = (ed.tv_sec - st.tv_sec) + (ed.tv_usec - st.tv_usec) / 1000000.0;
    logMessage("Test summary:");
    logMessage("->Total time: %lfs", elapsed);
    logMessage("->Bytes received: %ld", byteReceived);
    logMessage("->Bandwidth: %lfBytes/sec", byteReceived / elapsed);
    logMessage("Transfer complete.\n");
}
