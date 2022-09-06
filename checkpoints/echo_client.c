//
// Created by Chengke on 2019/10/22.
// Modified by Chengke on 2021/08/26.
//

#include "unp.h"

const char* message = "hello\n"
"world\n\n\n"
"Looooooooooooooooooooooooooooooooooooooooooooong\n"
"what\na\nt\ne\nr\nr\ni\nb\nl\ne\n"
"\n\n\n";

#define MSG_LEN 15000
char message_buf[MSG_LEN];

void populate_buf() {
  int i;
  int message_len = strlen(message);
  memcpy(message_buf, message, message_len);
  i = message_len;
  while (i + 1 < MSG_LEN) {
    message_buf[i] = 'a' + (i % 26);
    i += 1;
  }
  message_buf[i] = '\n';
}

void str_cli(FILE *fp, int sockfd, int sleep_) {
  char sendline[MAXLINE];
  char recvline[MAXLINE];
  while (fgets(sendline, MAXLINE, fp) != NULL) {
    writen(sockfd, sendline, strlen(sendline));
    if (sleep_) sleep(1);

    if (readline(sockfd, recvline, MAXLINE) == 0) {
      printf("str_cli: server terminated prematurely\n");
      exit(1);
    }

    // fputs(recvline, stdout);
  }
}

void cli_client(const char* addr, int sleep_) {
  int sockfd;
  struct sockaddr_in servaddr;
  FILE* fp;

  sockfd = Socket(AF_INET, SOCK_STREAM, 0);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(10086);
  Inet_pton(AF_INET, addr, &servaddr.sin_addr);

  Connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
  
  populate_buf();
  
  fp = fmemopen(message_buf, MSG_LEN, "r");
  // fp = stdin;
  str_cli(fp, sockfd, sleep_);
  fclose(fp);
  
  close(sockfd);
}

int main(int argc, char *argv[]) {
  int loop;
  
  if (argc != 2) {
    printf("usage: %s <IPaddress>\n", argv[0]);
    return -1;
  }
  
  for (loop = 0; loop < 3; loop++) {
    cli_client(argv[1], loop==0);
    printf("loop #%d ok.\n", loop + 1);
  }

  return 0;
}
