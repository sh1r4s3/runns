//TODO: deal with ret err
// Common definitions
#include "runns.h"

// Emit error message and exit
#define ERR(format, ...) \
      fprintf(stderr, "client.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
      ret = EXIT_FAILURE;


struct option opts[] =
{
  { .name = "help", .has_arg = 0, .flag = 0, .val = 'h' },
  { .name = "username", .has_arg = 1, .flag = 0, .val = 'u' },
  { .name = "netns", .has_arg = 1, .flag = 0, .val = 'n' },
  { .name = "program", .has_arg = 1, .flag = 0, .val = 'p' },
  { .name = "verbose", .has_arg = 0, .flag = 0, .val = 'v' },
  { .name = "stop", .has_arg = 0, .flag = 0, .val = 's' },
  { .name = "force-stop", .has_arg = 0, .flag = 0, .val = 'f' },
  { .name = "list", .has_arg = 0, .flag = 0, .val = 'l' },
  { 0, 0, 0, 0 }
};

extern char **environ;

void
help_me()
{
  const char *hstr = "client [options]\n" \
                     "Options:\n" \
                     "-h|--help\thelp\n" \
                     "-s|--stop\tstop daemon\n" \
                     "-f|--force-stop\tforce stop daemon (don't wait childs)\n" \
                     "-l|--list\tlist childs\n" \
                     "-u|--username\tusername to switch\n" \
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
  const char *optstring = "hu:n:p:vfsl";
  int opt;
  char verbose = 0;
  int sockfd = 0;
  int ret = EXIT_SUCCESS;

  if (argc <= 1)
  {
    ERR("For the help message try: client --help");
    goto _exit;
  }

  while ((opt = getopt_long(argc, argv, optstring, opts, 0)) != -1)
  {
    switch (opt)
    {
      case 'h':
        help_me();
        break;
      case 's':
        hdr.flag |= RUNNS_STOP;
        break;
      case 'f':
        hdr.flag |= RUNNS_FORCE_STOP;
        break;
      case 'l':
        hdr.flag |= RUNNS_LIST;
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
        ERR("Wrong option: %c", (char)opt);
        goto _exit;
    }
  }

  // Not allow empty strings
  if (!hdr.flag && (!user || !netns || !prog))
  {
    ERR("Please check that you set username, network namespace and program");
    goto _exit;
  }

  // Output parameters in the case of verbose option
  if (verbose && !hdr.flag)
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
  sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd == -1)
  {
    ERR("Something gone very wrong, socket = %d", sockfd);
    goto _exit;
  }
  if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
  {
    ERR("Can't connect to runns daemon");
    goto _exit;
  }

  write(sockfd, (void *)&hdr, sizeof(hdr));
  // Stop daemon
  if (hdr.flag <= (RUNNS_FORCE_STOP | RUNNS_STOP))
    goto _exit;
  // Print list of childs and exit
  if (hdr.flag & RUNNS_LIST)
  {
    int childs_run;
    struct runns_child child;
    read(sockfd, (void *)&childs_run, sizeof(childs_run));
    for (int i = 0; i < childs_run; i++)
    {
      read(sockfd, (void *)&child, sizeof(child));
      printf("%d [%d] %s\n", i, (int)child.pid, child.name);
    }
    goto _exit;
  }

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
  if (sockfd) close(sockfd);
  return ret;
}
