/*
 * vim:et:sw=2:
 *
 * Copyright (c) 2022-2025 Nikita (sh1r4s3) Ermakov <sh1r4s3@mail.si-head.nl>
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

#include "runns.h"

#define INLOG_NAME "librunns"
#define ENV_LISTNER "RUNNS_NETNS"
#define ENV_LISTNER_IPV6 "RUNNS_NETNS_IPV6"

// Emit log message
#define ERR(format, ...) \
    do { \
        fprintf(stderr, INLOG_NAME ":%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
        exit(0); \
    } while (0)

#define WARN(format, ...) \
    do { \
        fprintf(stderr, INLOG_NAME ":%d / warning / " format "\n", __LINE__, ##__VA_ARGS__); \
    } while (0)

#define INFO(format, ...) \
    do { \
        printf(INLOG_NAME ":%d / info / " format "\n", __LINE__, ##__VA_ARGS__); \
    } while (0)

#ifdef ENABLE_DEBUG
#  define DEBUG(format, ...) \
    do { \
        printf(INLOG_NAME ":%d / debug / " format "\n", __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#  define DEBUG(...)
#endif

static int (*bind_orig)(int, const struct sockaddr *, socklen_t) = NULL;
static int ns_def_fd = 0;
static int netns_size = 0;
static struct netns *ns = NULL;

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

static void add_netns(char *ip, sa_family_t family) {
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
    family = AF_INET;
    parse_l4_proto(l4_proto_str, &l4_proto, &family);

    inet_pton(family, ip, ns[netns_size].ip);
    ns[netns_size].port = atoi(port);
    ns[netns_size].fd = open(netns_path, 0);
    ns[netns_size].family = family;
    ns[netns_size].proto = l4_proto;
    DEBUG("%s adding port=%d ip=%s(0x%x), netns=%d, proto=%d", __func__, ns[netns_size].port, ip, *((int *)ns[netns_size].ip), ns[netns_size].fd, ns[netns_size].proto);
    ++netns_size;
}

__attribute__((constructor))
void librunns_init() {
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
    ns = malloc(sizeof(struct netns)*(nssz + nssz_ipv6));
    if (!ns) {
        ERR("The Void has opened and a null pointer emerged in our world");
        return;
    }
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
    for (int i = 0; i < nssz_ipv6; ++i) {
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
void librunns_deinit() {
    DEBUG("%s", __func__);
    close(ns_def_fd);
    for (int i = 0; i < netns_size; ++i) {
        close(ns[i].fd);
    }
    free(ns);
}

static void switch_ns(int sockfd, int ns_fd, sa_family_t family, int type) {
    int so_options_def[] = {SO_REUSEADDR, SO_REUSEPORT, SO_KEEPALIVE, SO_DONTROUTE};
    int ip_options_def[] = {IP_TRANSPARENT};
    int ipv6_options_def[] = {IPV6_TRANSPARENT};
    int tcp_options_def[] = {TCP_NODELAY, TCP_CORK, TCP_DEFER_ACCEPT, TCP_QUICKACK};
    int err;

    // Get CLOEXEC and NONBLOCK fd flags
    int flags = fcntl(sockfd, F_GETFD);
    if (flags < 0) {
        ERR("can't get a socket's fd flags, errno=%d", errno);
        return;
    }

    DEBUG("%s switching to netns_fd=%d for sockfd=%d(type=%d) fd_flags=%d", __func__, ns_fd, sockfd, type, flags);
    setns(ns_fd, CLONE_NEWNET);
    close(sockfd);

    int new_sockfd = socket(family, type, 0);
    if (fcntl(new_sockfd, F_SETFD, flags) < 0) {
        ERR("can't set the new socket's flags, errno=%d", errno);
        close(sockfd);
        return;
    }

    // SO socket options
    for (int iopt = 0; iopt < sizeof(so_options_def)/sizeof(int); ++iopt) {
        int arg;
        socklen_t arg_sz = sizeof(arg);
        err = getsockopt(new_sockfd, SOL_SOCKET, so_options_def[iopt], &arg, &arg_sz);
        if (err < 0) {
            WARN("can't get a socket option %d, errno=%d", so_options_def[iopt], errno);
            continue;
        }
        DEBUG("SOL_SOCKET: opt=%d int_value=%d size=%d", so_options_def[iopt], arg, arg_sz);
        err = setsockopt(new_sockfd, SOL_SOCKET, so_options_def[iopt], &arg, sizeof(arg));
        if (err < 0) {
            WARN("can't set a socket option %d, errno=%d", so_options_def[iopt], errno);
        }
    }
    // IP socket options
    int *ip_options = ip_options_def;
    int ip_level = IPPROTO_IP;
    int ip_options_len = sizeof(ip_options_def)/sizeof(int);
    char family_char = '4';
    if (family == AF_INET6) {
        ip_options = ipv6_options_def;
        ip_level = IPPROTO_IPV6;
        ip_options_len = sizeof(ipv6_options_def)/sizeof(int);
        family_char = '6';
    }
    for (int iopt = 0; iopt < ip_options_len; ++iopt) {
        int arg;
        socklen_t arg_sz = sizeof(arg);
        err = getsockopt(new_sockfd, ip_level, ip_options[iopt], &arg, &arg_sz);
        if (err < 0) {
            WARN("can't get a IPv%c option %d, errno=%d", family_char, ip_options[iopt], errno);
            continue;
        }
        DEBUG("IPPROTO_IPV%c: opt=%d int_value=%d size=%d", family_char, ip_options[iopt], arg, arg_sz);
        err = setsockopt(new_sockfd, ip_level, ip_options[iopt], &arg, sizeof(arg));
        if (err < 0) {
            WARN("can't set a IPv%c option %d, errno=%d", family_char, ip_options[iopt], errno);
        }
    }
    // TCP socket options
    if (type & SOCK_STREAM) {
        for (int iopt = 0; iopt < sizeof(tcp_options_def)/sizeof(int); ++iopt) {
            int arg;
            socklen_t arg_sz = sizeof(arg);
            err = getsockopt(new_sockfd, IPPROTO_TCP, ip_options_def[iopt], &arg, &arg_sz);
            if (err < 0) {
                WARN("can't get a TCP option %d, errno=%d", tcp_options_def[iopt], errno);
                continue;
            }
            DEBUG("IPPROTO_TCP: opt=%d int_value=%d size=%d", tcp_options_def[iopt], arg, arg_sz);
            err = setsockopt(new_sockfd, IPPROTO_TCP, ip_options_def[iopt], &arg, sizeof(arg));
            if (err < 0) {
                WARN("can't set a TCP option %d, errno=%d", tcp_options_def[iopt], errno);
            }
        }
    }
    // Fix socket fd
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
            int type;
            socklen_t length = sizeof(int);
            // Get SOCK type
            if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &type, &length) < 0) {
                WARN("can't get socket type, errno=%d", errno);
            }
            if (ns[i].proto != L4_PROTOCOL_UNK) {
                int good = 0;
                switch (ns[i].proto) {
                    case L4_PROTOCOL_TCP:
                        good = type & SOCK_STREAM;
                        break;
                    case L4_PROTOCOL_UDP:
                        good = type & SOCK_DGRAM;
                        break;
                }
                if (!good) {
                    DEBUG("wrong l4 proto=%d type=%d", ns[i].proto, type);
                    continue;
                }
            }

            DEBUG("%s switching netns to %d", __func__, ns[i].fd);
            switch_ns(sockfd, ns[i].fd, family, type);
            break;
        }
    }
    DEBUG("%s exiting with sockfd=%d", __func__, sockfd);
    int ret = bind_orig(sockfd, addr, addrlen);
    DEBUG("%s bind_orig ret=%d errno=%d", __func__, ret, errno);
    return ret;
}
