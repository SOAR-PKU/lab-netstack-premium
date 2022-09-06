//
// Created by Chengke on 2019/10/22.
//

#ifndef _GNU_SOURCE
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#endif

#include "unp.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>

ssize_t
readn(int fd, void* buff, size_t nbytes) {
  size_t nleft = nbytes;
  char *ptr = (char*) buff;

  while (nleft > 0) {
    int nread = read(fd, ptr, nleft);

    if (nread == 0) { // EOF
      return nbytes - nleft;
    }

    if (nread < 0) {
      if (errno != EINTR) {
        return -1;
      }
      nread = 0;
    }

    nleft -= nread;
    ptr += nread;
  }

  return nbytes;
}

ssize_t
writen(int fd, const void* buff, size_t nbytes) {
  size_t nleft = nbytes;
  const char *ptr = (const char *) buff;

  while (nleft > 0) {
    int nwritten = write(fd, ptr, nleft);

    if (nwritten == 0 || (nwritten < 0 && errno != EINTR)) {
      return -1;
    }

    nleft -= nwritten;
    ptr += nwritten;
  }
  return nbytes;
}

/* painfully slow version -- example only */
ssize_t
readline(int fd, void* buff, size_t maxlen) {
  char c;
  ssize_t rc;
  char *ptr = (char *) buff;
  ssize_t n;
  for (n = 1; n < maxlen; n++) {
    again:
      rc = read(fd, &c, 1);
      if (rc == 1) {
        *ptr++ = c;
        if (c == '\n') {
          break;
        }
      } else if (rc == 0) {
        *ptr = 0;
        return n - 1;
      } else {
        if (errno == EINTR) {
          goto again;
        }
        return -1;
      }
  }
  *ptr = 0;
  return n;
}

void Connect(int sockfd, struct sockaddr *addr, socklen_t len) {
  int rv = connect(sockfd, addr, len);
  if (rv < 0) {
    printf("connect failed %s\n", strerror(errno));
    exit(-1);
  }
}

void Inet_pton(int af, const char *src, void *dst) {
  int rv = inet_pton(af, src, dst);
  if (rv < 0) {
    printf("inet_pton failed %s\n", strerror(errno));
    exit(-1);
  }
}

int Socket(int domain, int type, int protocol) {
int rv = socket(domain, type, protocol);
  if (rv < 0) {
    printf("socket failed %s\n", strerror(errno));
    exit(-1);
  }
  return rv;
}

void Bind(int sockfd, struct sockaddr *addr, size_t len) {
  int rv = bind(sockfd, addr, len);
  if (rv < 0) {
    printf("bind failed %s\n", strerror(errno));
    exit(-1);
  }
}

int Accept(int sockfd, struct sockaddr *addr, socklen_t *len) {
  int rv = accept(sockfd, addr, len);
  if (rv < 0) {
    printf("accept failed %s\n", strerror(errno));
    exit(-1);
  }
  return rv;
}

void Listen(int sockfd, int backlog) {
  int rv = listen(sockfd, backlog);
  if (rv < 0) {
    printf("listen failed %s\n", strerror(errno));
    exit(-1);
  }
}

int Fork() {
  int rv = fork();
  if (rv < 0) {
    printf("fork failed %s\n", strerror(errno));
    exit(-1);
  }
  return rv;
}

void Setns(const char* name) {
  char buf[128];
  static const char *ns_path = "/var/run/netns/";
  memcpy(buf, ns_path, strlen(ns_path) + 1);
  strncat(buf, name, 64);
  int fd = open(buf, O_RDONLY);
  if (fd == -1) {
    printf("setns failed %s\n", strerror(errno));
    exit(-1);
  }
  /* Join that namespace */
  if (setns(fd, CLONE_NEWNET) == -1) {
    printf("setns failed %s\n", strerror(errno));
    exit(-1);
  }
}
