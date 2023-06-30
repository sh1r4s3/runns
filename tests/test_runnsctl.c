/*
 * vim:et:sw=2:
 *
 * Copyright (c) 2023 Nikita (sh1r4s3) Ermakov <sh1r4s3@mail.si-head.nl>
 * SPDX-License-Identifier: MIT
 */
#include <errno.h>
#include "tau/tau.h"
#include "runns.h"

TAU_MAIN();

TEST(runnsctl, add_netns) {
    extern int netns_size;
    extern struct netns *ns_head;
    void add_netns(char *ip);

    const char ip[] = "127.0.0.1:1234:/foo/bar:TCP4";
    add_netns(ip);
    REQUIRE(netns_size == 1);
}
