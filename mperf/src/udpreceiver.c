#include "util.h"

const char *usage = 
    "  -B [local ip]:\n"
    "    Specify the receiver ip address.\n"
    "  -h:\n"
    "    Print this message and exit.\n"
    "  -l [path]:\n"
    "    Specify the file to save the log.\n"
    "  -p [port]:\n"
    "    Specify port number of server.\n"
    "    *: Required\n"
    "  -v:\n"
    "    Print version information and exit.\n"
    "  -V[level]:\n"
    "    Print verbose log. Use -V2 for even more verbose log.";

#define MAX_ATTEMPT 20

static char *localIP = NULL;
static int port = 0;
static char *path;
static int connfd;
static struct sockaddr_in serverInfo, clientInfo;
static socklen_t len = sizeof(struct sockaddr_in);
static long sent, received;
static long recvBuf[8192];

static void parseArguments(int argc, char **argv)
{
    char c;
    optind = 0;
    while ((c = getopt(argc, argv, "B:p:vV::l:h")) != EOF)
    {
        switch (c)
        {
        case 'B':
            localIP = optarg;
            break;
        case 'h':
            printUsageAndExit(argv);
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'l':
            path = optarg;
            break;
        case 'v':
            printVersionAndExit("mperf-receiver");
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

static void initConnection()
{
	char errbuf[256];
	int attempt = 1;

	memset(&clientInfo, 0, sizeof(clientInfo));
	memset(&serverInfo, 0, sizeof(serverInfo));
	close(connfd);

	for (; attempt <= MAX_ATTEMPT; ++attempt)
	{
		if ((connfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		{
			logError("Attempt #%d failed(%s): Can't create socket!", attempt,
				strerrorV(errno, errbuf));
			continue;
		}

		serverInfo.sin_family = AF_INET;
		serverInfo.sin_port = htons(port);
		if (localIP == NULL)
		{
			serverInfo.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		else if (inet_aton(localIP, &serverInfo.sin_addr) == 0)
		{
			logFatal("Invalid server IP!");
		}

		if (bind(connfd, (struct sockaddr*)&serverInfo, len) == -1)
		{
			logError("Attempt %d failed(%s): Can't bind to port!", attempt,
				strerrorV(errno, errbuf));
			continue;
		}

		logMessage("Listening on %s:%d...", localIP, port);
		break;
	}

	if (attempt > MAX_ATTEMPT)
	{
		logFatal("%d fails in initConnection, exit.", attempt);
	}
}

int main(int argc, char **argv)
{
	int reinit = 1;
	char errbuf[256];

    if (argc == 1)
    {
        printUsageAndExit(argv);
    }

    initLog();

    parseArguments(argc, argv);
    printInitLog();

	while (1)
	{
		int size;
		
		if (reinit)
		{
			initConnection();
			reinit = 0;
		}
		
		if ((size = recvfrom(connfd, recvBuf, sizeof(long) * 8192, 0, 
			(struct sockaddr*)&clientInfo, &len)) == -1)
		{
			logError("Socket broken when receiving(%s), trying to restart...",
				strerrorV(errno, errbuf));
			reinit = 1;
			continue;
		}

		logMessage("Packet #%ld received from %s:%d(total %ld)", recvBuf[0],
			inet_ntoa(clientInfo.sin_addr), (int)clientInfo.sin_port,
			++received);

		if (sendto(connfd, recvBuf, size, 0, 
			(struct sockaddr *)&clientInfo, len) == -1)
		{
			logError("Socket broken when sending(%s), trying to restart...",
				strerrorV(errno, errbuf));
			reinit = 1;
			continue;
		}

		logMessage("Packet #%ld sent to %s:%d(total %ld)", recvBuf[0],
			inet_ntoa(clientInfo.sin_addr), (int)clientInfo.sin_port, ++sent);
	}

	close(connfd);
	return 0;
}
