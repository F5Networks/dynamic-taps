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
    struct event       *closeEvent;
    struct event       *sendEvent;
    struct event       *receiveEvent;
    struct event       *errorEvent;
    ClosedCb            closed;
    ConnectionErrorCb   connectionError;
    SentCb              sent;
    ExpiredCb           expired;
    SendErrorCb         sendError;
    ReceivedCb          received;
    ReceivedPartialCb   receivedPartial;
    ReceiveErrorCb      receiveError;
    /* Opaque pointers for TAPS */
    void               *taps_ctx;
    void               *send_ctx;
    void               *receive_ctx;
    void               *receive_buffer;
    size_t              receive_buffer_size;
};

struct listener_ctx {
    struct event_base    *base;
    struct event         *event;
    evutil_socket_t       fd;
    ConnectionReceivedCb  connectionReceived;
    EstablishmentErrorCb  establishmentError;
    ClosedCb              closed;
    ConnectionErrorCb     connectionError;
    void                 *taps_ctx;
};

static void
_tcp_closed(evutil_socket_t sock, short event, void *arg)
{
    struct conn_ctx *cctx = arg;

    event_del(cctx->closeEvent);
    event_free(cctx->closeEvent);
    event_del(cctx->sendEvent);
    event_free(cctx->sendEvent);
    event_del(cctx->receiveEvent);
    event_free(cctx->receiveEvent);
    close(cctx->fd);
    (cctx->closed)(cctx->taps_ctx);
    free(cctx);
}

static void
_tcp_sent(evutil_socket_t sock, short event, void *arg)
{
    struct conn_ctx *c = arg;

    (c->sent)(c->send_ctx);
}

static void
_tcp_received(evutil_socket_t sock, short event, void *arg)
{
    ssize_t bytes;
    struct conn_ctx *c = arg;

    bytes = read(c->fd, c->receive_buffer, c->receive_buffer_size);
    printf("read %lu bytes\n", bytes);
    if (bytes < 0) {
        /* XXX Call receiveError */
        return;
    }
    if (bytes == 0) {
        return;
    }
    (c->receivedPartial)(c->receive_ctx, c->receive_buffer, bytes);
}

static void
_tcp_connection_received(evutil_socket_t listener, short event, void *arg)
{
    struct listener_ctx     *lctx = arg;
    struct sockaddr_storage  ss;
    socklen_t                slen = sizeof(ss);
    struct conn_ctx         *cctx;

    //TAPS_TRACE();
    cctx = malloc(sizeof(struct conn_ctx));
    if (!cctx) goto fail;
    cctx->fd = accept(listener, (struct sockaddr *)&ss, &slen);
    if (cctx->fd < 0) goto fail;
    evutil_make_socket_nonblocking(cctx->fd);
    cctx->base = lctx->base;
    cctx->closeEvent = event_new(cctx->base, cctx->fd, EV_CLOSED,
            &_tcp_closed, cctx);
    cctx->sendEvent = event_new(cctx->base, cctx->fd, EV_WRITE, _tcp_sent,
            cctx);
    cctx->receiveEvent = event_new(cctx->base, cctx->fd, EV_READ,
            _tcp_received, cctx);
    cctx->errorEvent = NULL;
    if (event_add(cctx->closeEvent, NULL) < 0) {
        printf("TCP could not add closed event\n");
        event_free(cctx->closeEvent);
    }
    cctx->closed = lctx->closed;
    cctx->connectionError = lctx->connectionError;
    cctx->taps_ctx = (lctx->connectionReceived)(lctx->taps_ctx, cctx);
    if (!cctx->taps_ctx) {
        goto fail;
    }
    return;
fail:
    if (cctx) {
        close(cctx->fd);
        free(cctx);
    }
    return;
}

void *
Listen(void *taps_ctx, struct event_base *base, struct sockaddr *local,
        ConnectionReceivedCb connectionReceived,
        EstablishmentErrorCb establishmentError,
        ClosedCb closed, ConnectionErrorCb connectionError)
{
    struct listener_ctx *listener;
    size_t               addr_size = (local->sa_family == AF_INET) ?
            sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

    //TAPS_TRACE();
    listener = malloc(sizeof(struct listener_ctx));
    if (!listener) return NULL;
    listener->base = base;
    listener->event = NULL;
    listener->fd = socket(local->sa_family, SOCK_STREAM, 0);
    if (listener->fd < 0) goto fail;
    listener->connectionReceived = connectionReceived;
    listener->establishmentError = establishmentError;
    listener->closed = closed;
    listener->connectionError = connectionError;
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
            EV_READ | EV_PERSIST, _tcp_connection_received, (void *)listener);
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
Stop(void *proto_ctx, StoppedCb cb)
{
    struct listener_ctx *ctx = proto_ctx;
    void  *taps_ctx = ctx->taps_ctx;

    //TAPS_TRACE();
    event_del(ctx->event);
    event_free(ctx->event);
    close(ctx->fd);
    free(proto_ctx);
    /* Thread should be dead */
    (*cb)(ctx->taps_ctx);
}

int
Send(void *proto_ctx, void *taps_ctx, struct iovec *message, int iovcnt,
        SentCb sent, ExpiredCb expired, SendErrorCb sendError)
{
    struct conn_ctx    *c = proto_ctx;
    ssize_t             retval;

    if (!c->sent) {
        c->sent = sent;
        c->expired = expired;
        c->sendError = sendError;
    }
    if (event_pending(c->sendEvent, EV_WRITE, NULL)) {
        printf("Sending with event pending!\n");
        return -1;
    }
    c->send_ctx = taps_ctx;
    /* Ready to send */
    retval = writev(c->fd, message, iovcnt);
    if (retval < 0) {
        return -1;
    }
    if (event_add(c->sendEvent, NULL) < 0) { /* XXX Add timeouts */
        return -1;;
    }
    return retval;
}

int
Receive(void *proto_ctx, void *taps_ctx, void *buf, size_t buf_size,
        ReceivedCb received, ReceivedPartialCb receivedPartial,
        ReceiveErrorCb receiveError)
{
    struct conn_ctx    *c = proto_ctx;
    struct iovec       *iovec;
    int                 iov_len;
    ssize_t             retval;

    if (!c->received) {
        c->received = received;
        c->receivedPartial = receivedPartial;
        c->receiveError = receiveError;
    }
    if (event_pending(c->receiveEvent, EV_READ, NULL)) {
        printf("Two TCP recv at once\n");
        return -1;
    }
    c->receive_ctx = taps_ctx;
    if (event_add(c->receiveEvent, NULL) < 0) { /* XXX Add timeouts */
        return -1;;
    }
    c->receive_buffer = buf;
    c->receive_buffer_size = buf_size;
    return 0;
}
