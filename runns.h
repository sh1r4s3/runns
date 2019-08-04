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

// Linux socket default file.
#define RUNNS_DIR "/var/run/runns/"
#define defsock RUNNS_DIR "runns.socket"

// Maximum number of childs
#define MAX_CHILDS 1024

// Definitions of the stop bits:
// RUNNS_STOP -- wait for childs to exit and then exit.
// RUNNS_LIST -- list childs runned by runns
#define RUNNS_STOP (int)1 << 1
#define RUNNS_LIST (int)1 << 2

// common header for server and client
struct runns_header
{
  size_t prog_sz;
  size_t netns_sz;
  size_t env_sz;
  size_t args_sz;
  unsigned int flag;
};

struct runns_child
{
  uid_t uid;
  pid_t pid;
};
#endif
