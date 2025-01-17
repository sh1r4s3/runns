/*
 * vim:et:sw=2:
 *
 * Copyright (c) 2023-2025 Nikita (sh1r4s3) Ermakov <sh1r4s3@mail.si-head.nl>
 * SPDX-License-Identifier: MIT
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <mqueue.h>

struct queue {
  char *name;
  mqd_t desc;
};

struct queue_msg {
  char *msg;
  size_t size;
  struct queue_msg *pnext;
};

typedef enum {
  QUEUE_OK,
  QUEUE_ERROR
} QUEUE_RET;

struct queue *queue_open(const char *name);
QUEUE_RET queue_close(struct queue *queue);
QUEUE_RET queue_send(struct queue *queue, struct queue_msg *msgs);
struct queue_msg *queue_recv(struct queue *queue);
QUEUE_RET queue_free_msgs(struct queue_msg *msgs);

#endif
