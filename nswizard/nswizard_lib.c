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
#define ENV_LISTNER "NSWIZARD_NETNS"
#define ENV_LISTNER_IPV6 "NSWIZARD_NETNS_IPV6"
#define ENV_SEPARATOR ';'

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
    unsigned char ip[sizeof(struct in6_addr)];
    int fd;
    sa_family_t family; // AF_INET or AF_INET6
    in_port_t port;
};

static int (*bind_orig)(int, const struct sockaddr *, socklen_t) = NULL;
static int ns_def_fd = 0;
static int netns_size = 0;
static struct netns *ns = NULL;

static void add_netns(char *ip, sa_family_t family) {
    char *port = NULL, *netns_path = NULL;
    port = strchr(ip, ENV_SEPARATOR);
    if (port) netns_path = strchr(port + 1, ENV_SEPARATOR);
    if (!port || !netns_path) {
        DEBUG("%s can't parse %s", __func__, ip);
        return;
    }
    *port++ = '\0';
    *netns_path++ = '\0';

    inet_pton(family, ip, ns[netns_size].ip);
    ns[netns_size].port = atoi(port);
    ns[netns_size].fd = open(netns_path, 0);
    ns[netns_size].family = family;
    DEBUG("%s adding port=%d ip=%s(0x%x), netns=%d", __func__, ns[netns_size].port, ip, *((int *)ns[netns_size].ip), ns[netns_size].fd);
    ++netns_size;
}

__attribute__((constructor))
void wizard_lib_init() {
    DEBUG("%s", __func__);
    char buf[255];
    bind_orig = dlsym(RTLD_NEXT, "bind");
    ns_def_fd = open("/proc/self/ns/net", 0);

    int nssz = 0, nssz_ipv6 = 0;
    // Scan for IPv4
    while (1) {
        sprintf(buf, ENV_LISTNER "_%d", nssz);
        char *env = getenv(buf);
        if (!env) break;
        ++nssz;
    }
    // Scan for IPv6
    while (1) {
        sprintf(buf, ENV_LISTNER_IPV6 "_%d", nssz_ipv6);
        char *env = getenv(buf);
        if (!env) break;
        ++nssz_ipv6;
    }

    DEBUG("found nssz=%d nssz_ipv6=%d", nssz, nssz_ipv6);
    ns = malloc(sizeof(ns)*(nssz + nssz_ipv6)); // TODO: check for NULL
    for (int i = 0; i < nssz; ++i) {
        sprintf(buf, ENV_LISTNER "_%d", i);
        DEBUG("%s buf is %s", __func__, buf);
        char *ip = getenv(buf);
        if (!ip) {
            DEBUG("%s not found", buf);
            break;
        }
        add_netns(ip, AF_INET);
    }
    for (int i = 0; i < nssz; ++i) {
        sprintf(buf, ENV_LISTNER_IPV6 "_%d", i);
        DEBUG("%s buf is %s", __func__, buf);
        char *ip = getenv(buf);
        if (!ip) {
            DEBUG("%s not found", buf);
            break;
        }
        add_netns(ip, AF_INET6);
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
    setsockopt(new_sockfd, SOL_SOCKET, SO_REUSEADDR, s, sizeof(s));
    if (new_sockfd != sockfd) {
        DEBUG("new_sockfd != sockfd");
        dup2(new_sockfd, sockfd);
        close(new_sockfd);
    }
    setns(ns_def_fd, CLONE_NEWNET);
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    DEBUG("%s enter with sockfd=%d", __func__, sockfd);
    sa_family_t family;
    switch (addrlen) {
        case sizeof(struct sockaddr_in):
            DEBUG("%s family AF_INET", __func__);
            family = AF_INET;
            break;
        case sizeof(struct sockaddr_in6):
            DEBUG("%s family AF_INET6", __func__);
            family = AF_INET6;
            break;
        default:
            DEBUG("%s skip sockfd=%d", __func__, sockfd);
            return bind_orig(sockfd, addr, addrlen);
    }

    const struct sockaddr_in *ipaddr = (const struct sockaddr_in *)addr;
    for (int i = 0; i < netns_size; ++i) {
        DEBUG("%s checking i=%d", __func__, i);
        if (ns[i].family != family || ns[i].port != ntohs(ipaddr->sin_port)) continue;
        void *ip = family == AF_INET ?
                           (void *)&((const struct sockaddr_in *)addr)->sin_addr :
                           (void *)&((const struct sockaddr_in6 *)addr)->sin6_addr;
        DEBUG("%s cmp 0x%x with 0x%x", __func__, *((int *)ns[i].ip), *((int *)ip));
        if (memcmp((void *)ns[i].ip, ip, family == AF_INET ? sizeof(struct in_addr) : sizeof(struct in6_addr)) == 0) {
            DEBUG("%s switching netns to %d", __func__, ns[i].fd);
            switch_ns(sockfd, ns[i].fd);
            break;
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
