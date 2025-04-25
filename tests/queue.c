/*
 * vim:et:sw=2:
 *
 * Copyright (c) 2023-2025 Nikita Ermakov <sh1r4s3@pm.me>
 * SPDX-License-Identifier: MIT
 */
#include <errno.h>
#include "tau/tau.h"
#include "queue.h"

TAU_MAIN();

TEST(queue, open_close) {
  const char qname[] = "/runnsqueue";
  struct queue *q = queue_open(qname);
  REQUIRE(q, "queue_open() returns NULL ptr");
  QUEUE_RET ret = queue_close(q);
  REQUIRE(ret == QUEUE_OK);
}

TEST(queue, send_recv) {
  const char qname[] = "/runnsqueue";
  const char test_msg[] = "test message";
  pid_t pid = fork();
  REQUIRE(pid >= 0, "fork failed");

  if (pid == 0) { // child
    struct queue *q = queue_open(qname);
    REQUIRE(q, "queue_open() returns NULL ptr");

    struct queue_msg msg = {0};
    msg.msg = strdup(test_msg);
    msg.size = sizeof(test_msg);
    QUEUE_RET ret = queue_send(q, &msg);
    CHECK(ret == QUEUE_OK, "Can't send message");
    free(msg.msg);

    ret = queue_close(q);
    REQUIRE(ret == QUEUE_OK);
  } else { // parent
    struct queue *q = queue_open(qname);
    REQUIRE(q, "queue_open() returns NULL ptr");

    struct queue_msg *msgs = queue_recv(q);
    CHECK(msgs != NULL, "Received NULL");
    if (msgs != NULL) {
      CHECK(strcmp(msgs->msg, test_msg) == 0, "Messages are not equal");
      CHECK(msgs->pnext == NULL, "pnext is not NULL");
      queue_free_msgs(msgs);
    }

    QUEUE_RET ret = queue_close(q);
    REQUIRE(ret == QUEUE_OK);
  }

}
