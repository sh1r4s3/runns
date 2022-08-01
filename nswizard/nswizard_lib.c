/*
 * vim:et:sw=2:
 *
 * Copyright (c) 2022 Nikita (sh1r4s3) Ermakov <sh1r4s3@mail.si-head.nl>
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define INLOG_NAME "libnswizard"
#define ENV_NUM_LISTNERS "NSWIZARD_N"
#define ENV_LISTNER "NSWIZARD_NETNS"

// Emit log message
#define ERR(format, ...) \
    do { \
        printf(INLOG_NAME ":%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
        exit(0); \
    } while (0)

#define WARN(format, ...) \
    do { \
        printf(INLOG_NAME ":%d / warning / " format "\n", __LINE__, ##__VA_ARGS__); \
    } while (0)

#define INFO(format, ...) \
    do { \
        printf(INLOG_NAME ":%d / info / " format "\n", __LINE__, ##__VA_ARGS__); \
    } while (0)

#ifdef PRINT_DEBUG
#  define DEBUG(format, ...) \
    do { \
        printf(INLOG_NAME ":%d / debug / " format "\n", __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#  define DEBUG(...)
#endif

struct netns {
    int fd;
    in_port_t port;
    struct in_addr ip;
};

static int (*bind_orig)(int, const struct sockaddr *, socklen_t) = NULL;
static int ns_def_fd = 0;
static int netns_size = 0;
static struct netns *ns = NULL;

__attribute__((constructor))
void wizard_lib_init() {
    DEBUG("%s", __func__);
    bind_orig = dlsym(RTLD_NEXT, "bind");
    ns_def_fd = open("/proc/self/ns/net", 0);

    char *nlsts_str = getenv(ENV_NUM_LISTNERS);
    if (!nlsts_str) ERR(ENV_NUM_LISTNERS " does not defined");
    // TODO: check for valid input in the env var
    int nlsts = atoi(nlsts_str);
    netns_size = nlsts;
    ns = malloc(sizeof(ns)*nlsts); // TODO: check for NULL
    char buf[255];
    for (int i = 0; i < nlsts; ++i) {
        sprintf(buf, ENV_LISTNER "_%d", i);
        DEBUG("%s buf is %s", __func__, buf);
        char *ip = getenv(buf);
        if (!ip) {
            DEBUG("%s not found", buf);
            continue;
        }
        char *port = strchr(ip, ':');
        char *netns_path = strchr(port + 1, ':');
        *port++ = '\0';
        *netns_path++ = '\0';

        inet_aton(ip, &ns[i].ip);
        ns[i].port = atoi(port);
        ns[i].fd = open(netns_path, 0);
    }
}

__attribute__((destructor))
void wizard_lib_deinit() {
    DEBUG("%s", __func__);
    close(ns_def_fd);
    for (int i = 0; i < netns_size; ++i) {
        close(ns[i].fd);
    }
    free(ns);
}

static void switch_ns(int sockfd, int ns_fd) {
    DEBUG("%s switching to netns_fd=%d for sockfd=%d", __func__, ns_fd, sockfd);
    setns(ns_fd, CLONE_NEWNET);
    close(sockfd);
    int new_sockfd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0);
    int s[] = {1};
    setsockopt(new_sockfd, SOL_SOCKET, SO_REUSEADDR, s, 4);
    if (new_sockfd != sockfd) {
        DEBUG("new_sockfd != sockfd");
        dup2(new_sockfd, sockfd);
        close(new_sockfd);
    }
    setns(ns_def_fd, CLONE_NEWNET);
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    DEBUG("%s enter with sockfd=%d", __func__, sockfd);
    if (addrlen == sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *ipaddr = (const struct sockaddr_in *)addr;
        for (int i = 0; i < netns_size; ++i) {
            DEBUG("%s testing %d:%d against %d:%d", __func__,
                  ipaddr->sin_addr.s_addr, ntohs(ipaddr->sin_port),
                  ns[i].ip.s_addr, ns[i].port);
            if (ns[i].ip.s_addr == ipaddr->sin_addr.s_addr && ns[i].port == ntohs(ipaddr->sin_port))
                switch_ns(sockfd, ns[i].fd);
        }
    }
    DEBUG("%s exiting with sockfd=%d", __func__, sockfd);
    int ret = bind_orig(sockfd, addr, addrlen);
    DEBUG("%s bind_orig ret=%d errno=%d", __func__, ret, errno);
    return ret;
}

#if 0
int puts(const char *s) {
    puts_orig("wizard");
    return puts_orig(s);
}
#endif
