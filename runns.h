#ifndef RUNNS_H
#define RUNNS_H

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

// Emit error message and exit
#define ERR(format, ...) \
    { \
      fprintf(stderr, "runns.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
      exit(EXIT_FAILURE); \
    }

// Emit warning message
#define WARN(format, ...) \
    fprintf(stderr, "runns.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__)

struct runns_header
{
  int usersz;
  int progsz;
};


#endif
