#ifndef __SNDRCV_H__
#define __SNDRCV_H__

#include "lock.h"

// sndrcv utils
extern lock_t sigalrm;
extern lock_t sigint;

#ifdef PROBE
void doProbe(int connfd, int sendInterval, int probeInterval, int size,
    int loop, char *packetBuf);
#else
void doFixTest(int connfd, int maxtime, int len, char *packetBuf);
#endif
void doLongTest(int connfd, int timelen, char *packetBuf);
void doReceive(int connfd, int timelen, char *recvBuf);

static inline int continueTest()
{
    return isLocked(&sigint) && isLocked(&sigalrm);
}

#endif
