// Common definitions
#include "runns.h"

struct option opts[] =
{
  { .name = "help", .has_arg = 0, .flag = 0, .val = 'h' },
  { .name = "username", .has_arg = 1, .flag = 0, .val = 'u' },
  { .name = "netns", .has_arg = 1, .flag = 0, .val = 'n' },
  { .name = "program", .has_arg = 1, .flag = 0, .val = 'p' },
  { .name = "verbose", .has_arg = 0, .flag = 0, .val = 'v' },
  { .name = "stop", .has_arg = 0, .flag = 0, .val = 's' },
  { .name = "force-stop", .has_arg = 0, .flag = 0, .val = 'f' },
  { 0, 0, 0, 0 }
};

extern char **environ;

void
help_me()
{
  const char *hstr = "client [options]\n" \
                     "Options:\n" \
                     "-s|--stop\tstop daemon\n" \
                     "-f|--force-stop\tforce stop daemon (don't wait childs)\n" \
                     "-u|--username\tusername to switch\n" \
                     "-h|--help\thelp\n" \
                     "-n|--netns\tnetwork namespace to switch\n" \
                     "-p|--program\tprogram to run in desired netns\n" \
                     "-v|--verbose\tbe verbose\n";

  puts(hstr);
  exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
  struct runns_header hdr = {0};
  struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = defsock};
  const char *user = 0, *prog = 0, *netns = 0;
  const char *optstring = "hu:n:p:vfs";
  int opt;
  char verbose = 0;

  if (argc <= 1)
  {
    printf("For the help message try: client --help\n");
    return EXIT_FAILURE;
  }

  while ((opt = getopt_long(argc, argv, optstring, opts, 0)) != -1)
  {
    switch (opt)
    {
      case 'h':
        help_me();
        break;
      case 's':
        hdr.stopbit = RUNNS_STOP;
        break;
      case 'f':
        hdr.stopbit = RUNNS_FORCE_STOP;
        break;
      case 'u':
        user = optarg;
        hdr.user_sz = strlen(user) + 1;
        break;
      case 'n':
        netns = optarg;
        hdr.netns_sz = strlen(netns) + 1;
        break;
      case 'p':
        prog = optarg;
        hdr.prog_sz = strlen(prog) + 1;
        break;
      case 'v':
        verbose = 1;
        break;
      default:
        ERR(0, "client.c", "How did you do that? 0x%X %c", opt, (char)opt);
    }
  }

  // Not allow empty strings
  if (!hdr.stopbit && (!user || !netns || !prog))
    ERR(0, "client.c", "Please check that you set username, network namespace and program");

  // Output parameters in the case of verbose option
  if (verbose && !hdr.stopbit)
  {
    printf("user name to switch is: %s\n" \
           "network namespace to switch is: %s\n" \
           "program to run: %s\n", \
           user, netns, prog);
  }

  // Count number of environment variables
  for (size_t i = 0; environ[i] != 0; i++)
  {
    if (environ[i][0] != '_' && environ[i][1] != '=')
      hdr.env_sz++;
  }

  // Up socket
  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd == -1)
    ERR(sockfd, "client.c", "Something gone very wrong, socket = %d", sockfd);
  if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
    ERR(sockfd, "client.c", "Can't connect");

  write(sockfd, (void *)&hdr, sizeof(hdr));
  if (hdr.stopbit)
    goto _exit;


  write(sockfd, (void *)user, hdr.user_sz);
  write(sockfd, (void *)prog, hdr.prog_sz);
  write(sockfd, (void *)netns, hdr.netns_sz);

  // Transfer environment variables
  for (int i = 0; i < hdr.env_sz; i++)
  {
    if (environ[i][0] != '_' && environ[i][1] != '=')
    {
      size_t sz = strlen(environ[i]) + 1; // strlen + \0
      write(sockfd, (void *)&sz, sizeof(size_t));
      write(sockfd, (void *)environ[i], sz);
    }
  }
  int eof = 0;
  write(sockfd, &eof, sizeof(int));

_exit:
  close(sockfd);
  return EXIT_SUCCESS;
}
