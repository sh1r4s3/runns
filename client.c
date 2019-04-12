#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define ERR(format, ...) \
    { fprintf(stderr, "runns.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
    exit(EXIT_FAILURE); }

#define WARN(format, ...) \
    fprintf(stderr, "runns.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__)

int
main(int argc, char **argv)
{
  struct runns_header
  {
    int usersz;
    int progsz;
  } hdr;
  char *username = "nik";
  char *program = "/usr/bin/ip";
  struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = "/tmp/runns.socket"};

  // Up socket
  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd == -1)
    ERR("Something gone very wrong, socket = %d", sockfd);

  if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
    ERR("Can't connect");

  hdr.usersz = strlen(username) + 1;
  hdr.progsz = strlen(program) + 1;
  write(sockfd, (void *)&hdr, sizeof(hdr));
  write(sockfd, (void *)username, strlen(username) + 1);
  write(sockfd, (void *)program, strlen(program) + 1);

  close(sockfd);

  return 0;
}
