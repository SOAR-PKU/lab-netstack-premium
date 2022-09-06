//
// Created by Chengke on 2021/08/26.
//

#include "unp.h"
#include <sys/time.h>

#define SIZE (1460 * 100)
char sendline[SIZE];
char recvline[SIZE];

double timeval_subtract(struct timeval *x, struct timeval *y)
{
  double diff = x->tv_sec - y->tv_sec;
  diff += (x->tv_usec - y->tv_usec) / 1000000.0;
  return diff;
}

void fill_line() {
  int i;
  for (i = 0; i < SIZE; i++) {
    sendline[i] = 'a' + rand() % 26;
  }
}

void cmp_line() {
  int i;
  for (i = 0; i < SIZE; i++) {
    if (sendline[i] != recvline[i]) {
      printf("diff at [%d]\n", i);
      printf("send: %d, receive %d\n", (int) sendline[i], (int) recvline[i]);
      return;
    }
  }
}

int main(int argc, char *argv[]) {
  int sockfd;
  struct sockaddr_in servaddr;
  struct timeval start_ts, end_ts;
  double v, t;
  int loop;
  
  if (argc != 2) {
    printf("usage: %s <IPaddress>\n", argv[0]);
    return -1;
  }

  sockfd = Socket(AF_INET, SOCK_STREAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(10086);
  Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

  Connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

  for (loop = 0; loop < 10; loop++) {
    gettimeofday(&start_ts,NULL);

    fill_line();
    printf("sending ...\n");
    if (writen(sockfd, sendline, SIZE) < 0) {
      printf("writen error\n");
    }
    printf("receiving ...\n");
    if (readn(sockfd, recvline, SIZE) != SIZE) {
      printf("readn error\n");
    }

    gettimeofday(&end_ts,NULL);

    t = timeval_subtract(&end_ts, &start_ts);
    v = SIZE / t;
    printf("%.2lf KB/s\n", v / 1000);

    cmp_line();
  }
  
  return 0;
}
