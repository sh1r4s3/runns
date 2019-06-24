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

// Emit error message and exit
#define ERR(fd, file, format, ...) \
    { \
      fprintf(stderr, file":%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
      if (fd > 0)   { close(fd); unlink(RUNNS_DIR); } \
      exit(EXIT_FAILURE); \
    }

// Emit warning message.
#define WARN(format, ...) \
    fprintf(stderr, "runns.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__)

// Definitions of the stop bits:
// RUNNS_STOP -- wait for childs to exit and then exit.
// RUNNS_FORCE_STOP -- do not wait for childs, just exit.
// RUNNS_LIST -- list childs runned by runns
#define RUNNS_STOP 0x1
#define RUNNS_FORCE_STOP 0x2
#define RUNNS_LIST 0x4
#define RUNNS_NORMALIZE 0xEF // All except RUNNS_STOP.

// common header for server and client
struct runns_header
{
  size_t user_sz;
  size_t prog_sz;
  size_t netns_sz;
  size_t env_sz;
  int flag;
};

struct runns_child
{
  pid_t pid;
  const char *name;
};
#endif
