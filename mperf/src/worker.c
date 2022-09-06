#include "sndrcv.h"
#include "util.h"

static const char *svusage = 
    "  -h:\n"
    "    Print this message and exit.\n"
    "  -l [path]:\n"
    "    Specify the file to save the log.\n"
    "  -p [port]:\n"
    "    Specify port number of server.\n"
    "    *: Required\n"
    "  -s [source IP]:\n"
    "    Specify the source IP to listen on.\n"
    "    (default: unspecified)\n"
    "  -V[level]:\n"
    "    Print verbose log. Use -V2 for even more verbose log.";

static char type = TYPE_FIX;
static int arg = 1024;
static int arg2 = 200;
static char packetBuf[PACKET_LEN];

static int running = 0;

static int port;
static char *path;
static char* sourceIP;

static void configure()
{
    static char message[256];
    int targ, ttype, targ2;
    int pid = getppid();

    logMessage("Trying to reconfigure server(%d).", getpid());
    if (getMessage(SMEM_MESSAGE, message) < 0)
    {
        logError("Can't receive arguments: shared memory not initialized.");
        goto configure_fail_out;
    }

    sscanf(message, "%d%d%d", &ttype, &targ, &targ2);
    switch (ttype & ~FLAG_REVERSE)
    {
    case TYPE_LONG:
        arg = targ;
        type = (char)ttype;
        if (arg <= 0)
        {
            logError("Invalid long test time %d", arg);
            goto configure_fail_out;
        }
        logMessage("Reconfigured with type = long, reverse = %d, "
            "timeout = %d", BOOL(ttype & FLAG_REVERSE), targ);
        break;
    case TYPE_FIX:
        arg = targ;
        arg2 = targ2;
        type = (char)ttype;
        if (arg <= 0)
        {
            arg = 200;
        }
        if (targ2 <= 0)
        {
            logError("Invalid fix test size %d", targ2);
            goto configure_fail_out;
        }
#ifdef PROBE
        logMessage("Reconfigured with type = fix, reverse = %d, "
            "loop = %d, sendInterval = %d, probeInterval = %d, size = %d", 
            BOOL(ttype & FLAG_REVERSE), targ, (targ2 >> 24) & 0xFF,
            targ2 & 0xFFFF, (targ2 >> 16) & 0xFF);
#else
        logMessage("Reconfigured with type = fix, reverse = %d, "
            "timeout = %d, size = %d", BOOL(ttype & FLAG_REVERSE), targ, targ2);
#endif
        break;
    default:
        sprintf(message, "Unrecognized type %d", ttype);
        logWarning("%s", message);
        setMessage(SMEM_MESSAGE, message);
        goto configure_fail_out;
    }

    goto configure_out;

configure_fail_out:
    // send SIGUSR2 and exit
    if (rKill(pid, "controller", SIGUSR2) != RET_SUCC)
    {
        logError("Can't contact with controller!");
    }
    exit(0);
configure_out:
    if (rKill(pid, "controller", SIGUSR1) != RET_SUCC)
    {
        logError("Can't contact with controller!");
    }
}

static void sigintHandler(int sig)
{
    int be = errno;
    logVerbose("--SIGINT received.");
    // not running, we simply exit as expected.
    if (!running)
    {
        exit(0);
    }

    release(&sigint);
    errno = be;
}

static void sigalrmHandler(int sig)
{
    int be = errno;
    logVerbose("--SIGALRM received.");
    release(&sigalrm);
    errno = be;
}

static void parse(int connfd)
{
    switch (type)
    {
    case TYPE_LONG:
        doLongTest(connfd, arg, packetBuf);
        break;
    case TYPE_FIX:
#ifdef PROBE
        doProbe(connfd, (arg2 >> 24) & 0xFF, arg2 & 0xFFFF, (arg2 >> 16) & 0xFF,
            arg, packetBuf);
#else
        doFixTest(connfd, arg, arg2, packetBuf);
#endif
        break;
    case TYPE_REVLONG:
    case TYPE_REVFIX:
        doReceive(connfd, arg, packetBuf);
        break;
    default:
        logWarning("Unrecognized type %d.", (int)type);
    }
    running = 0;
}

static void parseArguments(int argc, char **argv)
{
    char c;
    optind = 0;
    while ((c = getopt(argc, argv, "p:vV::l:hs:")) != EOF)
    {
        switch (c)
        {
        case 'h':
            printUsageAndExit(argv);
            break;
        case 'l':
            path = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 's':
            sourceIP = optarg;
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

    if (port == 0)
    {
        logFatal("No legal port number specified.\n");
    }
    if (path != NULL)
    {
        redirectLogTo(path);
    }
}

int serverMain(int argc, char **argv)
{
    int listenfd;
    unsigned int clientlen;
    unsigned short clientport;
    int connfd = -1;
    struct sockaddr_in clientaddr;
    char *haddrp;
    usage = svusage;
    char errbuf[256];
#ifdef CHECK
    long i = 0;
#endif

    if (argc == 1)
    {
        printUsageAndExit(argv);
    }

    initLog();
    parseArguments(argc, argv);
    printInitLog();

#ifdef CHECK
    while (++i < (PACKET_LEN >> 3))
    {
        ((long*)packetBuf)[i] = i;
    }
#else
    memset(packetBuf, 0x10, sizeof(packetBuf));
#endif

    signalNoRestart(SIGALRM, sigalrmHandler);
    signalNoRestart(SIGINT, sigintHandler);
    signalNoRestart(SIGPIPE, SIG_IGN);

    configure();

    listenfd = forceOpenListenFD(sourceIP, port);

    logMessage("Listening on port %d.", port);
    clientlen = sizeof(clientaddr);

    setLock(&sigint);
    setLock(&sigalrm);
    running = 1;

    while (continueTest())
    {
        connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (connfd < 0)
        {
            if (errno == EINTR)
            {
                logVerbose("%s", "Accept interrupted.");
            }
            else
            {
                logError("Unable to accept connection(%s)!",
                         strerrorV(errno, errbuf));
            }
            continue;
        }
#ifdef FIX
        int flag = 1;
        setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
#endif
#ifdef PROBE
        int flag = 1;
        setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
#endif
        break;
    }

    if (!continueTest())
    {
        return 1;
    }

    if (close(listenfd) < 0)
    {
        logWarning("Error when closing listen socket(%s).", 
            strerrorV(errno, errbuf));
    }

    haddrp = inet_ntoa(clientaddr.sin_addr);
    clientport = clientaddr.sin_port >> 8 | clientaddr.sin_port << 8;
    logMessage("Connected with %s:%d", haddrp, (int)clientport);

    parse(connfd);
    if (close(connfd) < 0)
    {
        logWarning("Error when closing connection(%s).", 
            strerrorV(errno, errbuf));
    }
    logMessage("Connection with %s closed.\n", haddrp);

    return 0;
}

