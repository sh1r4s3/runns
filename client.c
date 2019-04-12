// Common definitions
#include "runns.h"


int
main(int argc, char **argv)
{
  struct runns_header hdr;
  struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = "/tmp/runns.socket"};
  const char *optstring = "h";
  char *username = "nik";
  char *program = "/usr/bin/ip";
  int opt;

  while ((opt = getopt(argc, argv, optstring)) != -1)
  {
    switch (opt)
    {
      case 'h':
        printf("help\n");
        break;
      default:
        printf("default\n");
    }
  }
  return 0;

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
