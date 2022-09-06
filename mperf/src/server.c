#include "util.h"

const char *usage =
    "  -h:\n"
    "    Print this message and exit.\n"
    "  -l [path]:\n"
    "    Specify the file to save the log of controller.\n"
    "  -L [path]:\n"
    "    Specify the file to save the log of server.\n"
    "  -k\n"
    "    Use SIGTERM(rather than SIGINT) to kill server if failed\n"
    "    (ensures that up to one child server process can be running at the\n" 
    "    same time. This improves robustness but can cause some logs from\n"
    "    server processes to be lost).\n"
    "  -p [port]:\n"
    "    Specify port number of controller.\n"
    "    *: Required\n"
    "  -P [Port]:\n"
    "    Specify port number of server.\n"
    "    (default: [port] - 1)\n"
    "  -s [source IP]:\n"
    "    Specify the source IP to listen on.\n"
    "    (default: unspecified)\n"
    "  -v:\n"
    "    Print version information and exit.\n"
    "  -V[level]:\n"
    "    Print verbose log. Use -V2 for even more verbose log.";

#define SV_RESPONSE 1
#define MAX_ARGS 256

static lock_t sigusr1;
static lock_t sigusr2;
static lock_t sigchld;
static lock_t sigalrm;

static int port;
static char *path;
static char* sourceIP;

static int svPort;
static char *svPath;
static char *svargv[MAX_ARGS];
static char **psvargv = svargv;

static int completeKill = 0;

static pid_t chldPID;

static int connfd;
static int listenfd;

extern int serverMain(int argc, char **argv);

static inline pid_t backupFork()
{
    return chldPID = fork();
}

static void sigusr1Handler(int sig)
{
    int be = errno;
    logVerbose("--SIGUSR1 received.");
    release(&sigusr1);
    errno = be;
}

static void sigusr2Handler(int sig)
{
    int be = errno;
    logVerbose("--SIGUSR2 received.");
    release(&sigusr2);
    errno = be;
}

static void sigalrmHandler(int sig)
{
    int be = errno;
    logVerbose("--SIGALRM received.");
    release(&sigalrm);
    errno = be;
}

static void sigchldHandler(int sig)
{
    int x;
    int be = errno;
    logVerbose("--SIGCHLD received.");
    while ((x = waitpid(0, NULL, WNOHANG)) > 0)
    {
        logMessage("--Child process(%d) terminated", x);
        if (x == chldPID)
        {
            chldPID = -1;
        }
    }
    if (chldPID != -1)
    {
        logError("--%d not terminated!", chldPID);
    }
    release(&sigchld);
    errno = be;
}

static char rTerminate()
{
    char ret;

    setLock(&sigchld);
    setLock(&sigalrm);
    alarmWithLog(SV_RESPONSE);

    if ((ret = rKill(chldPID, "server", SIGINT)) != RET_SUCC)
    {
        alarmWithLog(0);
        return ret;
    }

    spinAND(&sigchld, &sigalrm);
    alarmWithLog(0);

    if (isLocked(&sigchld))
    {
        logError(
            "Failed to terminate server with SIGINT(no response in 1 second)!");

        if (completeKill)
        {
            logMessage("Trying to terminate server with SIGTERM...");
            if ((ret = rKill(chldPID, "server", SIGTERM)) != RET_SUCC)
            {
                return ret;
            }
        }
    }

    return RET_SUCC;
}

static void doTerminate(int connfd)
{
    char ret = RET_SUCC;
    char buf[256];

    logMessage("Trying to terminate the server...");

    if (chldPID <= 0)
    {
        goto doTerminate_out;
    }

    ret = rTerminate();

doTerminate_out:
    rSendBytes(connfd, &ret, 1, "Failed to send return value to client");
    if (ret == RET_SUCC)
    {
        logMessage("Terminate completed.");
    }
    else
    {
        retstr(ret, buf);
        logWarning("Terminate failed(%s).", buf);
    }
}

static void pushArg(const char *arg)
{
    int len;
    if (*psvargv != NULL)
    {
        free(*psvargv);
    }
    len = strlen(arg) + 1;
    *psvargv = malloc(len);
    memcpy(*(psvargv++), arg, len);
}

static int buildArgs()
{
    char buf[16];
    psvargv = svargv;

    pushArg("server");
    if (verbose)
    {
        sprintf(buf, "-V%d", verbose);
        pushArg(buf);
    }
    if (svPath != NULL)
    {
        pushArg("-l");
        pushArg(svPath);
    }
    pushArg("-p");
    sprintf(buf, "%d", svPort);
    pushArg(buf);

    *psvargv = NULL;

    return psvargv - svargv;
}

static void startAsServer()
{
    int argc = buildArgs();
    
    forceClose(connfd);
    forceClose(listenfd);
    forceSignal(SIGUSR1, SIG_DFL);
    forceSignal(SIGCHLD, SIG_DFL);
    forceSignal(SIGALRM, SIG_DFL);
    forceSignal(SIGPIPE, SIG_DFL);

    logMessage("Server(%d) starting...", getpid());
    resetLogFile();

    exit(serverMain(argc, svargv));
}

static void doConfigure(int connfd)
{
    char ret;
    char message[1024];
    char errbuf[256];

    logMessage("Trying to reconfigure the server...");
    logVerbose("Existing child: %d", chldPID);

    if (chldPID > 0 && (ret = rTerminate()) != RET_SUCC)
    {
        goto doConfigure_out;
    }

    if ((ret = rReceiveMessage(connfd, "client", message)) != RET_SUCC)
    {
        goto doConfigure_out;
    }
    setMessage(SMEM_MESSAGE, message);
    logMessage("Reconfiguring, control message is \"%s\".", message);

    setLock(&sigusr1);
    setLock(&sigusr2);
    setLock(&sigalrm);
    alarmWithLog(SV_RESPONSE);

    if (backupFork() == 0)
    {
        startAsServer();
    }
    else if (chldPID == -1)
    {
        logError("Failed to start server(%s)!", strerrorV(errno, errbuf));
    }

    spinAND3(&sigusr1, &sigusr2, &sigalrm);
    alarmWithLog(0);

    if (!isLocked(&sigusr2))
    {
        getMessage(SMEM_MESSAGE, message);
        ret = RET_EMSG;
        goto doConfigure_out;
    }
    else if (!isLocked(&sigalrm))
    {
        ret = RET_EPROC;
        goto doConfigure_out;
    }

doConfigure_out:
    rSendBytes(connfd, &ret, 1, "Failed to send return value to client");
    if (ret == RET_EMSG)
    {
        logError("Server terminated(%s).", message);
        rSendMessage(connfd, "client", message, strlen(message) + 1);
    }
    if (ret == RET_SUCC)
    {
        logMessage("Reconfigure completed.");
    }
    else
    {
        logWarning("Reconfigure failed(%s).", retstr(ret, errbuf));
    }
}

static void parse(int connfd)
{
    char c;
    if (rRecvBytes(connfd, &c, 1, 
        "Failed to receive instruction from client") == RET_SUCC)
    {
        switch (c)
        {
        case SIG_TERM:
            doTerminate(connfd);
            break;
        case SIG_CONF:
            doConfigure(connfd);
            break;
        default:
            logWarning("Unrecognized instruction %d.", (int)c);
        }
    }
}

static void parseArguments(int argc, char **argv)
{
    char c;
    optind = 0;
    while ((c = getopt(argc, argv, "p:vV::l:hL:s:P:k")) != EOF)
    {
        switch (c)
        {
        case 'h':
            printUsageAndExit(argv);
            break;
        case 'k':
            completeKill = 1;
            break;
        case 'l':
            path = optarg;
            break;
        case 'L':
            svPath = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'P':
            svPort = atoi(optarg);
            break;
        case 's':
            sourceIP = optarg;
            break;
        case 'v':
            printVersionAndExit("mperf-server");
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
    if (svPort == 0)
    {
        svPort = port - 1;
    }
    if (path != NULL)
    {
        redirectLogTo(path);
    }
}

int main(int argc, char **argv)
{
    char errbuf[256];

    if (argc == 1)
    {
        printUsageAndExit(argv);
    }

    initLog();
    parseArguments(argc, argv);
    printInitLog();

    signalNoRestart(SIGUSR1, sigusr1Handler);
    signalNoRestart(SIGUSR2, sigusr2Handler);
    signalNoRestart(SIGCHLD, sigchldHandler);
    signalNoRestart(SIGALRM, sigalrmHandler);
    signalNoRestart(SIGPIPE, SIG_IGN);

    initSharedMem(SMEM_MESSAGE);

    listenfd = forceOpenListenFD(sourceIP, port);

    // initialize complete, start main loop
    while (1) 
    {
        unsigned int clientlen;
        struct sockaddr_in clientaddr;
        unsigned short clientport;
        char *haddrp;

        logMessage("Listening on port %d.", port);
        clientlen = sizeof(clientaddr);
        while ((connfd = accept(
            listenfd, (struct sockaddr *)&clientaddr, &clientlen)) < 0);
        {
            if (errno != 0)
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
            }
            logMessage("Listening on port %d.", port);
            clientlen = sizeof(clientaddr);
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
    }

    return 0;
}

