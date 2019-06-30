#ifndef RUNNS_H
#define RUNNS_H

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

// Definitions of the stop bits:
// RUNNS_STOP -- wait for childs to exit and then exit.
// RUNNS_FORCE_STOP -- do not wait for childs, just exit.
// RUNNS_LIST -- list childs runned by runns
#define RUNNS_STOP 0x1
#define RUNNS_FORCE_STOP 0x2
#define RUNNS_LIST 0x4

// common header for server and client
struct runns_header
{
  size_t user_sz;
  size_t prog_sz;
  size_t netns_sz;
  size_t env_sz;
  unsigned int flag;
};

struct runns_child
{
  pid_t pid;
  const char *name;
};
#endif
