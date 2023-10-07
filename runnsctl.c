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

void cleanup();

/*
 * Macros for logging
 */
#define ERR(format, ...) \
    do { \
        fprintf(stderr, CLIENT_NAME ":%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
        cleanup(); \
        exit(0); \
    } while (0)
#define WARN(format, ...) \
    do { \
        fprintf(stderr, CLIENT_NAME ":%d / warning / " format "\n", __LINE__, ##__VA_ARGS__); \
    } while (0)
#define INFO(format, ...) \
    do { \
        printf(CLIENT_NAME ":%d / info / " format "\n", __LINE__, ##__VA_ARGS__); \
    } while (0)
// This macro could be set from the configure script
#ifdef ENABLE_DEBUG
#  define DEBUG(format, ...) \
    do { \
        printf(CLIENT_NAME ":%d / debug / " format "\n", __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#  define DEBUG(...)
#endif

enum wide_opts {OPT_SET_NETNS = 0xFF01, OPT_SOCKET = 0xFF02};

int netns_size = 0;
struct netns_list *ns_head = NULL;
int sockfd = 0;

struct option opts[] = {
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

void help_me() {
  const char *hstr = \
"client [options]  \n"                                                \
"Options:  \n"                                                        \
"-h|--help             help\n"                                        \
"-s|--stop             stop daemon (only root)\n"                     \
"-l|--list             list childs\n"                                 \
"-p|--program <path>   program to run in desired netns\n"             \
"-t|--create-ptms      create control terminal\n"                     \
"-f|--forward-port     <ip>:<port>:<netns path>:<proto><ip family>\n" \
"                      <ip family> could be 4 or 6\n"                 \
"                      <netns path> path to the netns fd\n"           \
"--set-netns <path>    network namespace to switch\n"                 \
"--socket <path>       path to the runns socket\n"                    \
"-v|--verbose          be verbose\n";

  puts(hstr);
  exit(EXIT_SUCCESS);
}

void parse_l4_proto(char *l4_proto_str, L4_PROTOCOLS *l4_proto, sa_family_t *family) {
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
    if (strncasecmp(l4_proto_str, "tcp", 3) == 0) {
        *l4_proto = L4_PROTOCOL_TCP;
    } else if (strncasecmp(l4_proto_str, "udp", 3) == 0) {
        *l4_proto = L4_PROTOCOL_UDP;
    } else {
        WARN("L4 protocol %s is not correct", l4_proto_str);
    }

    DEBUG("l4_proto_str=%s l4_proto=%d family=%d", l4_proto_str, l4_proto, family);
}

void add_netns(char *ip) {
    if (!ip) {
        ERR("forward string is empty");
    }
    char *port = NULL, *netns_path = NULL;
    size_t ip_len = strlen(ip);
    port = strchr(ip, ENV_SEPARATOR);
    if (port && (size_t)port < ((size_t)ip + ip_len))
        netns_path = strchr(port + 1, ENV_SEPARATOR);
    if (!port || !netns_path) { // Mandatory fields
        WARN("skipping %s -- port and netns_path are mandatory fields", ip);
        return;
    }
    *port++ = '\0';
    *netns_path++ = '\0';
    // An optional field for level 4 protocols according to the OSI model
    char *l4_proto_str = strchr(netns_path + 1, ENV_SEPARATOR);
    L4_PROTOCOLS l4_proto = L4_PROTOCOL_UNK;
    sa_family_t family = AF_UNSPEC;
    parse_l4_proto(l4_proto_str, &l4_proto, &family);

    // Open a netns file to get a fd
    int netns_fd = open(netns_path, 0);
    if (netns_fd < 0) {
        ERR("Can't open netns %s errno: %s", netns_path, strerror(errno));
    }

    // Create a new netns
    struct netns_list *ns = (struct netns_list *)malloc(sizeof(struct netns_list));
    inet_pton(family, ip, ns->node.ip);
    ns->node.port = atoi(port);
    ns->node.fd = netns_fd;
    ns->node.family = family;
    ns->node.proto = l4_proto;
    ns->pnext = NULL;
    DEBUG("adding port=%d ip=%s(0x%x), netns=%s, proto=%d", ns->node.port, ip, *((int *)ns->node.ip), ns->node.netns, ns->node.proto);
    ++netns_size;
    // Insert into the list of netns
    if (ns_head) {
      struct netns_list *p;
      for (p = ns_head; p != NULL; p = p->pnext);
      p->pnext = ns;
    } else {
      ns_head = ns;
    }
}

void cleanup() {
  for (struct netns_list *p = ns_head; p != NULL;) {
    struct netns_list *ptmp = p->pnext;
    free(p);
    p = ptmp;
  }

  if (sockfd)
    close(sockfd);
}

// Don't define main() for unit tests
#ifndef TAU_TEST
int main(int argc, char **argv) {
  struct runns_header hdr = {0};
  struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = DEFAULT_RUNNS_SOCKET};
  const char *prog = 0, *netns = 0;
  const char *optstring = "hp:vsltf:";
  int opt, len;
  char verbose = 0;
  int ret = EXIT_SUCCESS;

  // Parse command line options
  if (argc <= 1)
    ERR("For the help message try: runnsctl --help");

  while ((opt = getopt_long(argc, argv, optstring, opts, 0)) != -1) {
    switch (opt) {
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
        if (hdr.op_mode == OP_MODE_NETNS) {
            ERR("--forward-port and --set-netns mutually exclusive");
        }
        hdr.op_mode = OP_MODE_FWD_PORT;
        add_netns(optarg);
        break;
      case 'v':
        verbose = 1;
        break;
      case OPT_SET_NETNS:
        if (hdr.op_mode == OP_MODE_FWD_PORT) {
            ERR("--forward-port and --set-netns mutually exclusive");
        }
        hdr.op_mode = OP_MODE_NETNS;
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

  // TODO: remove this debugging output
#if 0
  if (hdr.op_mode == OP_MODE_FWD_PORT) {
    for (struct netns *p = ns_head; p != NULL; p = p->pnext) {
      INFO("0x%x %s %s %d %d", *((int *)p->ip), p->netns, p->family == AF_INET ? "AF_INET" : "AF_INET6", p->proto, p->port);
    }
    exit(0);
  }
#endif

  if (hdr.op_mode == OP_MODE_FWD_PORT && !ns_head) {
    ERR("Nothing to forward");
  }
  if (hdr.op_mode == OP_MODE_NETNS && !hdr.flag && (!netns || !prog)) {
    ERR("Please check that you set network namespace and program");
  }

  // Output parameters in the case of verbose option
  if (netns && verbose) {
    if (hdr.flag & (RUNNS_STOP | RUNNS_LIST)) { // flags related to runns
                                                // daemon
        char *str = NULL;

        if (hdr.flag & RUNNS_STOP) str = "RUNNS_STOP";
        if (hdr.flag & RUNNS_LIST) str = "RUNNS_LIST";
        if (str)
            printf("Command to runns daemon: %s\n", str);
    } else {
        printf("network namespace to switch is: %s\n"
               "program to run: %s\n",
               netns, prog);
    }
  }

  // Count number of environment variables
  for (hdr.env_sz = 0; environ[hdr.env_sz] != 0; ++hdr.env_sz);

  // Up socket
  sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd == -1) {
    ERR("Something gone very wrong, socket = %d", sockfd);
    cleanup();
    return ret;
  }
  if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
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
  if (hdr.flag & RUNNS_LIST) {
    unsigned int childs_run;
    struct runns_child child;
    if (read(sockfd, (void *)&childs_run, sizeof(childs_run)) == -1)
      ERR("Can't read number of childs from the daemon");
    for (int i = 0; i < childs_run; i++) {
      if (read(sockfd, (void *)&child, sizeof(child)) == -1)
        ERR("Can't read child info from the daemon");
      printf("%d\n", child.pid);
    }
    cleanup();
    return ret;
  }

  // TODO: either transer prog + netns or a list of netns
  // this should depend on the current operation mode
  // OP_MODE_FWD_PORT -- forward ports (a list of netns)
  // OP_MODE_NETNS -- the "classic" mode to run prog in netns
  if (write(sockfd, (void *)prog, hdr.prog_sz) == -1 ||
      write(sockfd, (void *)netns, hdr.netns_sz) == -1) {

    ERR("Can't send program name or network namespace name to the daemon");
  }
  // Transfer argv
  if (hdr.args_sz > 0) {
    for (int i = optind; i < argc; i++) {
      size_t sz = strlen(argv[i]) + 1; // strlen + \0
      if (write(sockfd, (void *)&sz, sizeof(size_t)) == -1 ||
          write(sockfd, (void *)argv[i], sz) == -1) {

        ERR("Can't send argv to the daemon");
      }
    }
  }

  // Transfer environment variables
  for (int i = 0; i < hdr.env_sz; i++) {
    size_t sz = strlen(environ[i]) + 1; // strlen + \0
    if (write(sockfd, (void *)&sz, sizeof(size_t)) == -1 ||
        write(sockfd, (void *)environ[i], sz) == -1) {

      ERR("Can't send envs to the daemon");
    }
  }
  int eof = 0;
  if (write(sockfd, &eof, sizeof(int)) == -1)
    ERR("Can't send EOF to the daemon");

  cleanup();
  return ret;
}
#endif
