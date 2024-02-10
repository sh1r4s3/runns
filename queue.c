/*
 * vim:et:sw=2:
 *
 * Copyright (c) 2023-2024 Nikita (sh1r4s3) Ermakov <sh1r4s3@mail.si-head.nl>
 * SPDX-License-Identifier: MIT
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

struct queue *queue_open(const char *name) {
  int exists;
  struct queue *ptr = (struct queue *)malloc(sizeof(struct queue));
  if (!ptr)
    return NULL;

  // Set a queue entry
  struct mq_attr attr = {
    .mq_flags = 0,
    .mq_maxmsg = 16,
    .mq_msgsize = 1024,
    .mq_curmsgs = 0
  };
  ptr->name = strdup(name);
  ptr->desc = mq_open(name, O_RDWR | O_CREAT, 0666, &attr);
  if (ptr->desc == (mqd_t)-1)
    return NULL;

  return ptr;
}

QUEUE_RET queue_close(struct queue *queue) {
  free(queue->name);
  int ret = mq_close(queue->desc);
  int _errno = errno;
  free(queue);
  errno = _errno;
  return ret < 0 ? QUEUE_ERROR : QUEUE_OK;
}

QUEUE_RET queue_send(struct queue *queue, struct queue_msg *msgs) {
  for (struct queue_msg *ptr = msgs; ptr != NULL; ptr = ptr->pnext) {
    if (mq_send(queue->desc, ptr->msg, ptr->size, 0) < 0) {
      return QUEUE_ERROR;
    }
  }
  return QUEUE_OK;
}

struct queue_msg *queue_recv(struct queue *queue) {
  struct mq_attr attr;
  struct queue_msg *msg_head = NULL, **msg_tail = &msg_head;
  if (mq_getattr(queue->desc, &attr) < 0)
    return NULL;

  while (attr.mq_curmsgs--) {
    char *ptr = (char *)malloc(attr.mq_msgsize);
    size_t sz = mq_receive(queue->desc, ptr, attr.mq_msgsize, NULL);
    *msg_tail = (struct queue_msg *)malloc(sizeof(struct queue_msg));
    (*msg_tail)->msg = ptr;
    (*msg_tail)->size = sz;
    (*msg_tail)->pnext = NULL;
    msg_tail = &((*msg_tail)->pnext);
  }
  return msg_head;
}

QUEUE_RET queue_free_msgs(struct queue_msg *msgs) {
  struct queue_msg *ptr = msgs, *ptr_next;
  do {
    ptr_next = ptr->pnext;
    free(ptr->msg);
    free(ptr);
    ptr = ptr_next;
  } while(ptr);
}
