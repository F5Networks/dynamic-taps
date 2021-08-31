/*
 * Copyright 2021 F5 Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors to this project must sign and submit the Contributor License
 * Agreement available in this repository.
 */

/* tcp.c */
/* Wrap TCP sockets in a standardized taps interface */
#include <errno.h>
#include <event2/event.h>
#include <event2/util.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "../taps_protocol.h"

#define TAPS_TCP_DEFAULT_MAX_LISTEN 100

uint32_t taps_tcp_max_conns = 100;
uint32_t num_conns = 0;

#if 0
int
initiate(struct sockaddr *remote, char *local, void *transportProps,
        void *securityProps)
{
    int                  s;
    int                  domain = (int)(remote->sa_family);
//    transportProperties *tp = (transportProperties *)transportProps;
    /* don't need security props */

    if ((domain != AF_INET) && (domain != AF_INET6)) {
        errno = EINVAL;
        return -1;
    }
    s = socket(domain, SOCK_STREAM, 0);
    if (s == -1) {
        return s;
    }
    /* Set socket options */
    if (local != NULL) {
        if (setsockopt(s, getprotoent("TCP"), SO_BINDTODEVICE, local,
                    IFNAMSIZ) < 0) {
            goto fail;
        }
    }
    if (connect(s, remote, sizeof(*remote)) < 0) {
        goto fail;
    }
    return s;
fail:
    close(s);
    return -1;
}
#endif

/* Connection context */
struct conn_ctx {
    int                 fd;
    struct event_base  *base;
};

struct listener_ctx {
    struct event_base    *base;
    struct event         *event;
    evutil_socket_t       fd;
    pthread_t             thread;
    ConnectionReceivedCb  newConn;
    EstablishmentErrorCb  error;
    void                 *taps_ctx;
};

static void
newConn(evutil_socket_t listener, short event, void *arg)
{
    struct listener_ctx     *lctx = arg;
    struct sockaddr_storage  ss;
    socklen_t                slen = sizeof(ss);
    struct conn_ctx         *cctx;

    cctx = malloc(sizeof(struct conn_ctx));
    if (!cctx) goto fail;
    cctx->fd = accept(listener, (struct sockaddr *)&ss, &slen);
    if (cctx->fd < 0) goto fail;
    evutil_make_socket_nonblocking(cctx->fd);
    (lctx->newConn)(lctx->taps_ctx, cctx);
    return;
fail:
    if (cctx) {
        free(cctx);
    }
    return;
}

void *
Listen(void *taps_ctx, struct event_base *base, struct sockaddr *local,
        ConnectionReceivedCb newConnCb,
        EstablishmentErrorCb error)
{
    struct listener_ctx *listener;
    size_t               addr_size = (local->sa_family == AF_INET) ?
            sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

    listener = malloc(sizeof(struct listener_ctx));
    if (!listener) return NULL;
    listener->base = base;
    listener->event = NULL;
    listener->fd = socket(local->sa_family, SOCK_STREAM, 0);
    if (listener->fd < 0) goto fail;
    listener->newConn = newConnCb;
    listener->error = error;
    listener->taps_ctx = taps_ctx;
    evutil_make_socket_nonblocking(listener->fd);

#ifndef WIN32
    {
        int one = 1;
        setsockopt(listener->fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    }
#endif

    if (bind(listener->fd, local, addr_size) < 0) {
        printf("TCP bind failed: %s\n", strerror(errno));
        goto fail;
    }
    if (listen(listener->fd, TAPS_TCP_DEFAULT_MAX_LISTEN) < 0) {
        printf("TCP listen failed\n");
        goto fail;
    }
    listener->event = event_new(listener->base, listener->fd,
            EV_READ | EV_PERSIST, newConn, (void *)listener);
    event_add(listener->event, NULL);

    return listener;
fail:
    /* listener must exist to get here */
    free(listener);
    if (listener->fd > -1) {
        close(listener->fd);
    }
    if (listener->event) {
        event_free(listener->event);
    }
    return NULL;
}

int
ListenStop(void *proto_ctx, StoppedCb cb)
{
    struct listener_ctx *ctx = proto_ctx;

    event_del(ctx->event);
    event_free(ctx->event);
    ctx->event = NULL;
    /* Thread should be dead */
    (*cb)(ctx->taps_ctx);
    close(ctx->fd);
    free(proto_ctx);
}
