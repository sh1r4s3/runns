/*
 * vim:et:sw=2:
 *
 * Copyright (c) 2019-2022 Nikita (sh1r4s3) Ermakov <sh1r4s3@mail.si-head.nl>
 * SPDX-License-Identifier: MIT
 */

#ifndef RUNNS_H
#define RUNNS_H

#define __USE_GNU
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>

#define STR_TOKEN(x) #x

// Linux socket default file.
#define DEFAULT_RUNNS_SOCKET "/var/run/runns/runns.socket"
#define RUNNS_MAXLEN sizeof(((struct sockaddr_un *)0)->sun_path)

// Maximum number of childs
#define MAX_CHILDS 1024

// Definitions of the flag bits:
// RUNNS_STOP -- wait for childs to exit and then exit.
// RUNNS_LIST -- list childs runned by runns.
// RUNNS_NPTS -- create control terminal for forked process.
#define RUNNS_STOP        (int)1 << 1
#define RUNNS_LIST        (int)1 << 2
#define RUNNS_NPTMS       (int)1 << 3

// common header for server and client
struct runns_header
{
  size_t prog_sz;
  size_t netns_sz;
  size_t env_sz;
  size_t args_sz;
  unsigned int flag;
  struct termios tmode;
};

struct runns_child
{
  uid_t uid;
  pid_t pid;
};
#endif
