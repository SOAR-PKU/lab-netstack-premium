#include "util.h"

const char *usage = 
    "  -B [local ip]:\n"
    "    Specify the sender ip address.\n"
    "  -c [server ip]:\n"
    "    Specify the receiver ip address.\n"
    "    *: Required\n"
    "  -h:\n"
    "    Print this message and exit.\n"
	"  -i:\n"
	"    Specify the time interval(us) between two send operations.\n"
	"    (default: 10000).\n"
	"  -n: [count]\n"
	"    Send [count] packets and exit(default is keep sending).\n"
    "  -l [path]:\n"
    "    Specify the file to save the log.\n"
    "  -p [port]:\n"
    "    Specify port number of server.\n"
    "    *: Required\n"
	"  -s [size]:\n"
	"    Specify the number of 64-bit qword transferred in every packet\n"
	"    (default: 1, max 2048).\n"
	"  -t [time]\n"
	"    After sending all packets, wait [time] seconds and exit\n"
	"    (default is keep waiting)."
    "  -v:\n"
    "    Print version information and exit.\n"
    "  -V[level]:\n"
    "    Print verbose log. Use -V2 for even more verbose log.";

#define MAX_ATTEMPT 20

static unsigned short port = 0;
static char *path = NULL;
static char *localIP = NULL;
static char *serverIP = NULL;
static int connfd;
static struct sockaddr_in serverInfo;
static socklen_t len = sizeof(struct sockaddr_in);
// this works only on linux!
static struct timeval interval = { 0, -1 };
static fd_set wfds;
static long toSend = -1;
static int size = 1;
static long sent, received;
static long *sendBuf, *recvBuf;
static lock_t doingInit;
static int waitLen = -1;

static void parseArguments(int argc, char **argv)
{
    char c;
    optind = 0;
    while ((c = getopt(argc, argv, "s:B:c:p:i:l:n:vV::ht:")) != EOF)
    {
        switch (c)
        {
        case 'B':
            localIP = optarg;
            break;
        case 'c':
            serverIP = optarg;
            break;
        case 'h':
            printUsageAndExit(argv);
            break;
		case 'i':
			interval.tv_usec = atoi(optarg);
			break;
        case 'l':
            path = optarg;
            break;
		case 'n':
			toSend = atoi(optarg);
			break;
        case 'p':
            port = atoi(optarg);
            break;
		case 's':
			size = atoi(optarg);
			break;
        case 't':
            waitLen = atoi(optarg);
            break;
        case 'v':
            printVersionAndExit("mperf-sender");
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

	if (interval.tv_usec < 0)
	{
		interval.tv_usec = 10000;
	}
	interval.tv_sec = interval.tv_usec / (long)1000000;
	interval.tv_usec -= interval.tv_sec * (long)1000000;

	if (size < 2 || size > 2048)
	{
		size = 1;
	}
	sendBuf = (long*)malloc(sizeof(long) * size);
	recvBuf = (long*)malloc(sizeof(long) * size);
    if (serverIP == NULL)
    {
        logFatal("No server IP specified.");
    }
    if (port == 0)
    {
        logFatal("No legal port number specified.");
    }
    if (path != NULL)
    {
        redirectLogTo(path);
    }
}	

static void initConnection()
{
	int attempt = 1;
	char errbuf[256];

	memset(&serverInfo, 0, sizeof(serverInfo));
	close(connfd);

	for (; attempt <= MAX_ATTEMPT; ++attempt)
	{
    	if ((connfd = netdial(
        	AF_INET, SOCK_DGRAM, localIP, 0, serverIP, port)) < 0)
		{
			logError("initConnection failure #%d(%s): Can't create socket!", 
				attempt, strerrorV(errno, errbuf));
			continue;
		}

		serverInfo.sin_family = AF_INET;
		serverInfo.sin_port = htons(port);
		
		if (inet_aton(serverIP , &serverInfo.sin_addr) == 0) 
		{
			logError("initConnection failure #%d(%s): inet_aton failed!",
				attempt, strerrorV(errno, errbuf));
			continue;
		}

		logMessage("Socket initialized.");
		break;
	}

	if (attempt > MAX_ATTEMPT)
	{
		logFatal("%d attempts in initConnection, exit.", MAX_ATTEMPT);
	}
}

static inline void wInitSelect()
{
	FD_ZERO(&wfds);
	FD_SET(connfd, &wfds);
}

static void tryInitConnection()
{
	setLock(&doingInit);

	initConnection();

	release(&doingInit);
}

static void *do_receive(void *arg)
{
	int attempt = 1;
	char errbuf[256];

	while (received != toSend)
	{
		if (recvfrom(connfd, recvBuf, sizeof(long) * size, 0, 
			(struct sockaddr*)&serverInfo, &len) == -1)
		{
			logError("I/O failure #%d: Socket broken when receving(%s).",
				attempt, strerrorV(errno, errbuf));
			if (++attempt > MAX_ATTEMPT)
			{
				logFatal("%d attempts in I/O, exit.", MAX_ATTEMPT);
			}
			tryInitConnection();
			continue;
		}

		logMessage("Packet #%ld received(total %ld).", *recvBuf, ++received);
		attempt = 1;
	}

	return NULL;
}

static void *do_send(void *arg)
{
	int attempt = 1;
	char errbuf[256];
	int ret;

	while (1)
	{
		struct timespec remain;
		struct timeval temp = interval;

		wInitSelect();

		ret = select(connfd + 1, NULL, &wfds, NULL, &temp);
		if (ret < 0)
		{
			logError("I/O failure #%d: Error when waiting for sending(%s).", 
				attempt, strerrorV(errno, errbuf));
			if (++attempt > MAX_ATTEMPT)
			{
				logFatal("%d attempts in I/O, exit.", attempt);
			}
			continue;
		}
		else if (ret > 0)
		{
			*sendBuf = ++sent;
			if (sendto(connfd, sendBuf, sizeof(long) * size, 0, 
				(struct sockaddr*)&serverInfo, len) == -1)
			{			
				logError("I/O failure #%d: Socket broken when sending(%s).",
					attempt, strerrorV(errno, errbuf));
				if (++attempt > MAX_ATTEMPT)
				{
					logFatal("%d attempts in I/O, exit.", MAX_ATTEMPT);
				}
				tryInitConnection();
				continue;
			}
		}
		else
		{
			logWarning("I/O attempt #%d: fd not writable after %ldms.",
				attempt,
				interval.tv_sec * 1000 + interval.tv_usec / 1000);
			if (++attempt > MAX_ATTEMPT)
			{
				logFatal("%d attempts in I/O, exit.", MAX_ATTEMPT);
			}
		}
		
		logMessage("Packet #%ld sent.", sent);
		if (sent == toSend)
		{
			break;
		}

		remain.tv_sec = temp.tv_sec;
		remain.tv_nsec = (long)1000 * temp.tv_usec;
		nanosleep(&remain, NULL);
		attempt = 1;
	}

	if (waitLen > 0)
	{
		alarmWithLog(waitLen);
	}
	return NULL;
}

void sigalrmHandler(int sig)
{
	logWarning("Timeout after %d second(s), exit.", waitLen);
	exit(0);
}

int main(int argc, char **argv)
{
	pthread_t receiver, sender;

    if (argc == 1)
    {
        printUsageAndExit(argv);
    }

    initLog();

	parseArguments(argc, argv);
	printInitLog();

	release(&doingInit);
	initConnection();

	signalNoRestart(SIGALRM, sigalrmHandler);

	pthread_create(&sender, NULL, do_send, NULL);
	pthread_create(&receiver, NULL, do_receive, NULL);

	pthread_join(receiver, NULL);
	pthread_join(sender, NULL);

	close(connfd);

	return 0;
}
