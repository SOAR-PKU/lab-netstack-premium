#include "sndrcv.h"
#include "util.h"

const char *usage = 
    "  -B [local ip]:\n"
    "    Specify the local ip address.\n"
    "  -b [local port]:\n"
    "    Specify the local port.\n"
    "    **: STILL DEVELOPPING, BUGGY!"
    "    (default: let system choose a port randomly).\n"
    "  -c [server ip]:\n"
    "    Specify the server ip address.\n"
    "    *: Required\n"
    "  -h:\n"
    "    Print this message and exit.\n"
#ifdef PROBE
    "  -i [interval]:\n"
	"    Specify the time interval(ms) between two send operations.\n"
	"    (default: 4).\n"
    "  -I [interval]:\n"
	"    Specify the time interval(ms) between two probe operations.\n"
	"    (default: 1000).\n"
#endif
    "  -l [path]:\n"
    "    Specify the file to save the log.\n"
#ifndef PROBE
    "  -n [size]:\n"
    "    Tell the program to do fix test with size=[size]Bytes.\n"
#else
    "  -L [count]:\n"
    "    Do [count] loop tests(default: 1).\n"
    "  -n [count]: \n"
    "    Send [count] probe packets per probe(default: 25).\n"
#endif
    "  -p [port]:\n"
    "    Specify port number of server.\n"
    "    *: Required\n"
    "  -P [cport]:\n"
    "    Specify port number of controller(default: [port] + 1).\n"
    "  -s:\n"
    "    If specified, let the client send data.\n"
#ifndef PROBE
    "  -t [time]:\n"
    "    Tell the program to do long test with timeLength=[time]seconds.\n"
    "  -T [Time]:\n"
    "    End test and exit after [Time] seconds.\n"
    "      for long tests, default: [time] + 10.\n"
    "      for fix tests, default: 200.\n"
#endif
    "  -v:\n"
    "    Print version information and exit.\n"
    "  -V[level]:\n"
    "    Print verbose log. Use -V2 for even more verbose log.";

static char *localIP = NULL;
static char *serverIP = NULL;
static unsigned short localPort = 0;
static unsigned short port = 0;
static unsigned short cport = 0;
static int timelen = -1;
static int localTime = -1;
static int size = -1;
static char *path = NULL;
static int connfd = -1;
static int mss = 0;
static char packetBuf[67108864];
//static int rwnd = 3145728;
static int reverse = 0;
#ifdef PROBE
static int sendInterval = 4;
static int probeInterval = 1000;
static int loop = 0;
#endif

static void parseArguments(int argc, char **argv)
{
    char c;
    optind = 0;
#ifdef PROBE
    while ((c = getopt(argc, argv, "B:b:c:hi:I:l:L:n:p:P:svV::")) != EOF)
#else
    while ((c = getopt(argc, argv, "B:b:c:hl:n:p:P:st:T:vV::")) != EOF)
#endif
    {
        switch (c)
        {
        case 'B':
            localIP = optarg;
            break;
        case 'b':
            localPort = atoi(optarg);
            break;
        case 'c':
            serverIP = optarg;
            break;
        case 'h':
            printUsageAndExit(argv);
            break;
#ifdef PROBE
        case 'i':
            sendInterval = atoi(optarg);
            break;
        case 'I':
            probeInterval = atoi(optarg);
            break;
#endif
        case 'l':
            path = optarg;
            break;
#ifdef PROBE
        case 'L':
            loop = atoi(optarg);
            break;
#endif
        case 'n':
            size = atoi(optarg);
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'P':
            cport = atoi(optarg);
            break;
        case 's':
            reverse = FLAG_REVERSE;
            break;
#ifndef PROBE
        case 't':
            timelen = atoi(optarg);
            break;
        case 'T':
            localTime = atoi(optarg);
            break;
#endif
        case 'v':
            printVersionAndExit("mperf-client");
            break;
        case 'V':
            if (optarg)
            {
                setVerbose(atoi(optarg));
            }
            else
            {
                setVerbose(1);
            }
            break;
        default:
            logWarning("Unexpected command-line option %c!", (char)optopt);
            printUsageAndExit(argv);
        }
    }

    if (cport == 0)
    {
        cport = port + 1;
    }
    if (localTime < 0)
    {
        if (size > 0)
        {
            localTime = 200;
        }
        else
        {
            localTime = timelen + 10;
        }
    }

    if (serverIP == NULL)
    {
        logFatal("No server IP specified.");
    }
    if (port == 0)
    {
        logFatal("No legal port number specified.");
    }
#ifdef PROBE
    size &= 0xFF;
#else
    if (timelen < 0 && size < 0)
    {
        logFatal("No legal -t or -n argument specified.");
    }
#endif
    if (path != NULL)
    {
        redirectLogTo(path);
    }
#ifdef PROBE
    if (loop <= 0)
    {
        loop = 1;
    }
    sendInterval &= 0x7F;
    probeInterval &= 0xFFFF;
#endif
}

static inline void rRecvRetval(int connfd, const char *ope, char *ret)
{
    static char text[256];
    if (rio_readnr(connfd, ret, 1) < 1)
    {
        close(connfd);
        logFatal("%s: Can't receive return value!", ope);
    }
    if (*ret != RET_SUCC)
    {
        close(connfd);
        logFatal("%s failed(%s)!", ope, retstr(*ret, text));
    }
    
    logMessage("%s complete.", ope);
}

static void reconfigureServer()
{
    static char message[1024];
    int type = size > 0 ? TYPE_FIX : TYPE_LONG;
#ifdef PROBE
    int arg = loop;
    int arg2 = (sendInterval << 24) | (size << 16) | probeInterval;
#else
    int arg = size <= 0 && !reverse ? timelen : localTime;
    int arg2 = size;
#endif
    char ret;
    char errbuf[256];

    logVerbose("Trying to reconfigure the server...");
    // reconfigure now. 
    if ((connfd = netdial(
        AF_INET, SOCK_STREAM, localIP, localPort, serverIP, cport)) < 0)
    {
        logFatal("Can't connect to controller(%s)!", 
            strerrorV(errno, errbuf));
    }

    *message = SIG_CONF;
    if (rio_writenr(connfd, message, 1) < 1)
    {
        close(connfd);
        logFatal("Can't send instruction to controller(%s)!", 
            strerrorV(errno, errbuf));
    }
    sprintf(message, "%d %d %d", (int)type | reverse, arg, arg2);
    logVerbose("Control message: %s", message);
    rSendMessage(connfd, "controller", message, 1 + strlen(message));
    ret = rRecvBytes(connfd, &ret, 1, "Failed to receive return value");
    if (ret != RET_SUCC)
    {
        close(connfd);
        logFatal("Reconfigure failed(%s)!", retstr(ret, message));
    }
    close(connfd);
}

static void connectToServer()
{
    socklen_t socklen = sizeof(mss);
    if ((connfd = netdial(
        AF_INET, SOCK_STREAM, localIP, localPort, serverIP, port)) < 0)
    {
        logFatal("Can't connect to server!");
    }
    if (getsockopt(connfd, IPPROTO_TCP, 2, &mss, &socklen) < 0)
    {
        char errbuf[256];
        logWarning("Failed to get MSS(%s).", strerrorV(errno, errbuf));
    }
    else
    {
        logMessage("MSS is %d.", mss);
    }

#ifdef FIX
    int flag = 1;
    setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
#endif
#ifdef PROBE
    int flag = 1;
    setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
#endif
    logMessage("Connection established.");
}

void sigintHandler(int sig)
{
    int be = errno;
    logVerbose("--SIGINT received.");
    release(&sigint);
    errno = be;
}

void sigintHandlerEarly(int sig)
{
    logVerbose("--SIGINT received, exit.");
    close(connfd);
    exit(0);
}

void sigalrmHandler(int sig)
{
    int be = errno;
    logVerbose("--SIGALRM received.");
    release(&sigalrm);
    errno = be;
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printUsageAndExit(argv);
    }

    initLog();

    parseArguments(argc, argv);
    printInitLog();

    signalNoRestart(SIGINT, sigintHandlerEarly);
    reconfigureServer();
    connectToServer();

    signalNoRestart(SIGINT, sigintHandler);
    signalNoRestart(SIGALRM, sigalrmHandler);
    signalNoRestart(SIGPIPE, SIG_IGN);
    setLock(&sigint);
    setLock(&sigalrm);
    if (reverse)
    {
#ifdef PROBE
        doProbe(connfd, sendInterval, probeInterval, size, loop, packetBuf);
#else
        if (size > 0)
        {
            doFixTest(connfd, localTime, size, packetBuf);
        }
        else
        {
            doLongTest(connfd, timelen, packetBuf);
        }
#endif
    }
    else
    {
        doReceive(connfd, localTime, packetBuf);
    }
    return 0;
}
