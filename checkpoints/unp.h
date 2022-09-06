//
// Created by Chengke on 2019/10/22.
//

#ifndef EVAL_UNP_H
#define EVAL_UNP_H

#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <zconf.h>

#define MAXLINE 4096

ssize_t readn(int fd, void *buff, size_t nbytes);

ssize_t writen(int fd, const void *buff, size_t nbytes);

ssize_t readline(int fd, void *buff, size_t maxlen);


int Socket(int domain, int type, int protocol);

void Connect(int sockfd, struct sockaddr *addr, socklen_t len);

void Bind(int sockfd, struct sockaddr *addr, size_t len);

int Accept(int sockfd, struct sockaddr *addr, socklen_t *len);

void Listen(int sockfd, int backlog);

void Inet_pton(int af, const char *src, void *dst);

int Fork();

void Setns(const char *name);

#endif //EVAL_UNP_H
