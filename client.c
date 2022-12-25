/*
 * vim:et:sw=2:
 *
 * Copyright (c) 2019-2022 Nikita (sh1r4s3) Ermakov <sh1r4s3@mail.si-head.nl>
 * SPDX-License-Identifier: MIT
 */

#include "runns.h"
#include <arpa/inet.h>
#include <limits.h>

#define CLIENT_NAME "runnsctl"
#define ENV_SEPARATOR ';'

void cleanup();

#define ERR(format, ...) \
      fprintf(stderr, "client.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
      ret = EXIT_FAILURE;

enum wide_opts {OPT_SET_NETNS = 0xFF01, OPT_SOCKET = 0xFF02};
enum operation_mode {OP_MODE_UNK, OP_MODE_NETNS, OP_MODE_FWD_PORT};

static int netns_size = 0;
static struct netns *ns_head = NULL;
static enum operation_mode op_mode = OP_MODE_UNK;
static int sockfd = 0;

struct option opts[] =
{
  { .name = "help", .has_arg = 0, .flag = 0, .val = 'h' },
  { .name = "program", .has_arg = 1, .flag = 0, .val = 'p' },
  { .name = "verbose", .has_arg = 0, .flag = 0, .val = 'v' },
  { .name = "stop", .has_arg = 0, .flag = 0, .val = 's' },
  { .name = "list", .has_arg = 0, .flag = 0, .val = 'l' },
  { .name = "create-ptms", .has_arg = 0, .flag = 0, .val = 't' },
  { .name = "forward-port", .has_arg = 1, .flag = 0, .val = 'f' },
  { .name = "set-netns", .has_arg = 1, .flag = 0, .val = OPT_SET_NETNS },
  { .name = "socket", .has_arg = 1, .flag = 0, .val = OPT_SOCKET },
  { 0, 0, 0, 0 }
};

extern char **environ;

void
help_me()
{
  const char *hstr = \
"client [options]  \n"                                                \
"Options:  \n"                                                        \
"-h|--help             help  \n"                                      \
"-s|--stop             stop daemon (only root)  \n"                   \
"-l|--list             list childs  \n"                               \
"-p|--program <path>   program to run in desired netns  \n"           \
"-t|--create-ptms      create control terminal  \n"                   \
"-f|--forward-port     <ip>;<port>;<netns path>;<proto><ip family>\n" \
"                      <ip family> could be 4 or 6\n"                 \
"                      <netns path> path to the netns fd\n"           \
"--set-netns <path>    network namespace to switch  \n"               \
"--socket <path>       path to the runns socket  \n"                  \
"-v|--verbose          be verbose\n";

  puts(hstr);
  exit(EXIT_SUCCESS);
}

// TODO: check whether we need static here or it is redundant?
static inline void parse_l4_proto(char *l4_proto_str, L4_PROTOCOLS *l4_proto, sa_family_t *family) {
    // Basic checks and calc the size of the str
    if (!l4_proto_str || !l4_proto || !family) {
        ERR("NULL slipped in 0x%x 0x%x 0x%x", l4_proto_str, l4_proto, family);
    }
    *l4_proto_str++ = '\0';
    *family = AF_INET;
    *l4_proto = L4_PROTOCOL_UNK;
    size_t l4_proto_str_sz = strlen(l4_proto_str);
    if (l4_proto_str_sz != 4) {
        WARN("L4 protocol has wrong size %d != 4. Skip", l4_proto_str_sz);
        return;
    }
    // Get IP family
    switch (l4_proto_str[3]) {
        case '4':
            *family = AF_INET;
            break;
        case '6':
            *family = AF_INET6;
            break;
        default:
            WARN("L4 protocol has wrong IP family %c (only 4 and 6 are allowed). Fallback to 4", l4_proto_str[3]);
    }
    // Get proto
    if (strncmp(l4_proto_str, "tcp", 3) == 0) {
        *l4_proto = L4_PROTOCOL_TCP;
    } else if (strncmp(l4_proto_str, "udp", 3) == 0) {
        *l4_proto = L4_PROTOCOL_UDP;
    } else {
        WARN("L4 protocol %s is not correct");
    }

    DEBUG("l4_proto_str=%s l4_proto=%d family=%d", l4_proto_str, l4_proto, family);
}

static void add_netns(char *ip) {
    char *port = NULL, *netns_path = NULL;
    port = strchr(ip, ENV_SEPARATOR);
    if (port) netns_path = strchr(port + 1, ENV_SEPARATOR);
    if (!port || !netns_path) { // Mandatory fields
        DEBUG("%s can't parse %s", __func__, ip);
        return;
    }
    *port++ = '\0';
    *netns_path++ = '\0';
    // An optional field for level 4 protocols accocording to the OSI
    char *l4_proto_str = strchr(netns_path + 1, ENV_SEPARATOR);
    L4_PROTOCOLS l4_proto = L4_PROTOCOL_UNK;
    sa_family_t family = AF_UNSPEC;
    parse_l4_proto(l4_proto_str, &l4_proto, &family);

    // Create new
    struct netns *ns = (struct netns *)malloc(sizeof(struct netns));
    inet_pton(family, ip, ns->ip);
    ns->port = atoi(port);
    ns->netns = strndup(netns_path, PATH_MAX);
    ns->family = family;
    ns->proto = l4_proto;
    ns->pnext = NULL;
    DEBUG("%s adding port=%d ip=%s(0x%x), netns=%s, proto=%d", __func__, ns->port, ip, *((int *)ns->ip), ns->netns, ns->proto);
    ++netns_size;
    // Insert into list
    if (ns_head) {
      struct netns *p;
      for (p = ns_head; p != NULL; p = p->pnext);
      p->pnext = ns;
    } else {
      ns_head = ns;
    }
}

void cleanup() {
  for (struct netns *p = ns_head; p != NULL;) {
    struct netns *ptmp = p->pnext;
    free(p);
    p = ptmp;
  }

  if (sockfd)
    close(sockfd);
}

int
main(int argc, char **argv)
{
  struct runns_header hdr = {0};
  struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = DEFAULT_RUNNS_SOCKET};
  const char *prog = 0, *netns = 0, *args = 0;
  const char *optstring = "hp:vslt";
  int opt, len;
  char verbose = 0;
  int ret = EXIT_SUCCESS;

  if (argc <= 1)
    ERR("For the help message try: runnsctl --help");

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
      case 'l':
        hdr.flag |= RUNNS_LIST;
        break;
      case 'p':
        prog = optarg;
        hdr.prog_sz = strlen(prog) + 1;
        break;
      case 't':
        hdr.flag |= RUNNS_NPTMS;
        break;
      case 'f':
        if (op_mode == OP_MODE_NETNS) {
            ERR("--forward-port and --set-netns mutually exclusive");
        }
        op_mode = OP_MODE_FWD_PORT;
        add_netns(optarg);
        break;
      case 'v':
        verbose = 1;
        break;
      case OPT_SET_NETNS:
        if (op_mode == OP_MODE_FWD_PORT) {
            ERR("--forward-port and --set-netns mutually exclusive");
        }
        op_mode = OP_MODE_NETNS;
        netns = optarg;
        hdr.netns_sz = strlen(netns) + 1;
        break;
      case OPT_SOCKET:
        len = strlen(optarg);
        if (len >= RUNNS_MAXLEN)
          ERR("Socket file name is too long > " STR_TOKEN(RUNNS_MAXLEN) "\n");
        memcpy(addr.sun_path, optarg, len + 1);
        break;
      default:
        ERR("Wrong option: %c", (char)opt);
    }
  }

  // TODO
#if 0
  if (op_mode == OP_MODE_FWD_PORT) {
    for (struct netns *p = ns_head; p != NULL; p = p->pnext) {
      INFO("0x%x %s %s %d %d", *((int *)p->ip), p->netns, p->family == AF_INET ? "AF_INET" : "AF_INET6", p->proto, p->port);
    }
    exit(0);
  }
#endif

  // Not allow empty strings
  if (!hdr.flag && (!netns || !prog))
    ERR("Please check that you set network namespace and program");

  // Output parameters in the case of verbose option
  if (verbose && !hdr.flag)
  {
    printf("network namespace to switch is: %s\n" \
           "program to run: %s\n", \
           netns, prog);
  }

  // Count number of environment variables
  for (hdr.env_sz = 0; environ[hdr.env_sz] != 0; ++hdr.env_sz);

  // Up socket
  sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd == -1)
  {
    ERR("Something gone very wrong, socket = %d", sockfd);
    cleanup();
    return ret;
  }
  if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
  {
    ERR("Can't connect to runns daemon");
    cleanup();
    return ret;
  }
  // Calculate number of non-options
  hdr.args_sz = argc - optind;

  // Get termios
  tcgetattr(STDIN_FILENO, &hdr.tmode);

  if (write(sockfd, (void *)&hdr, sizeof(hdr)) == -1)
    ERR("Can't send header to the daemon");
  // Stop daemon
  if (hdr.flag & RUNNS_STOP) {
    cleanup();
    return ret;
  }
  // Print list of childs and exit
  if (hdr.flag & RUNNS_LIST)
  {
    unsigned int childs_run;
    struct runns_child child;
    if (read(sockfd, (void *)&childs_run, sizeof(childs_run)) == -1)
      ERR("Can't read number of childs from the daemon");
    for (int i = 0; i < childs_run; i++)
    {
      if (read(sockfd, (void *)&child, sizeof(child)) == -1)
        ERR("Can't read child info from the daemon");
      printf("%d\n", child.pid);
    }
    cleanup();
    return ret;
  }

  if (write(sockfd, (void *)prog, hdr.prog_sz) == -1 ||
      write(sockfd, (void *)netns, hdr.netns_sz) == -1)
  {
    ERR("Can't send program name or network namespace name to the daemon");
  }
  // Transfer argv
  if (hdr.args_sz > 0)
  {
    for (int i = optind; i < argc; i++)
    {
      size_t sz = strlen(argv[i]) + 1; // strlen + \0
      if (write(sockfd, (void *)&sz, sizeof(size_t)) == -1 ||
          write(sockfd, (void *)argv[i], sz) == -1)
      {
        ERR("Can't send argv to the daemon");
      }
    }
  }

  // Transfer environment variables
  for (int i = 0; i < hdr.env_sz; i++)
  {
    size_t sz = strlen(environ[i]) + 1; // strlen + \0
    if (write(sockfd, (void *)&sz, sizeof(size_t)) == -1 ||
        write(sockfd, (void *)environ[i], sz) == -1)
    {
      ERR("Can't send envs to the daemon");
    }
  }
  int eof = 0;
  if (write(sockfd, &eof, sizeof(int)) == -1)
    ERR("Can't send EOF to the daemon");

  cleanup();
  return ret;
}
