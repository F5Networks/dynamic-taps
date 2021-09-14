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

/*
 * 'echoapp' sets up a server on port 5555 that echoes each line of text the
 * client sends it.
 *
 * To test: telnet localhost 5555
 *
 * and type stuff
 *
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/taps.h"
#include "../src/taps_debug.h"

#define MIN_BUF  0
#define BUF_SIZE 1024
#define PRINT_RESULT(text, reason)    \
    if (reason) printf(text ": %s\n", reason); else printf(text "\n");

#define TAPS_DEBUG

#ifdef TAPS_DEBUG
#define TAPS_TRACE() printf("Function %s: %s\n", __FILE__, __FUNCTION__);
#else
#define TAPS_TRACE()
#endif

struct app_listener {
    struct event_base *base;
    TAPS_CTX          *taps;
    tapsCallbacks      callbacks;
};

struct app_conn {
    struct app_listener *l;
    TAPS_CTX            *taps;
};

struct app_msg {
    uint8_t              buf[BUF_SIZE];
    TAPS_CTX            *taps;
};

/* Callbacks */
static void
_app_send_error(void *conn, void *msg, char *reason)
{
    struct app_msg *m = msg;

    TAPS_TRACE();
    PRINT_RESULT("Sending failed", reason);
    tapsMessageFree(m->taps);
}

static void
_app_expired(void *conn, void *msg)
{
    struct app_msg *m = msg;

    TAPS_TRACE();
    tapsMessageFree(m->taps);
    free(m);
}

static void
_app_sent(void *conn, void *msg)
{
    struct app_msg *m = msg;

    TAPS_TRACE();
    tapsMessageFree(m->taps);
    free(m);
}

void _app_received_partial(void *conn, void *msg, size_t bytes, int eom);

static void
_app_received(void *conn, void *msg, size_t bytes)
{
    _app_received_partial(conn, msg, bytes, 1);
    /* XXX The only difference here is we've gotten FIN from the peer; so
       maybe we should call tapsConnectionClose()? */
}

void
_app_received_partial(void *conn, void *msg, size_t bytes, int eom)
{
    struct app_conn *c = conn;
    struct app_msg  *m = msg;
    void            *text;
    size_t           len;

    TAPS_TRACE();
    text = tapsMessageGetFirstBuf(m->taps, &len);
    printf("Received:\n%.*s", (int)bytes, (char *)text);
    tapsMessageTruncate(m->taps, bytes);
    if (tapsConnectionSend(c->taps, m->taps, m, &c->l->callbacks) < 0) {
        printf("Send failed\n");
        tapsMessageFree(m->taps);
        free(m);
    }
    /* Get ready for more! */
    m = malloc(sizeof(struct app_msg));
    if (!m) {
        printf("malloc failed, not receiving anymore\n");
        exit(-1);
    }
    m->taps = tapsMessageNew(m->buf, BUF_SIZE);
    if (m->taps) {
        tapsConnectionReceive(c->taps, m, m->taps, MIN_BUF, BUF_SIZE,
                &c->l->callbacks);
    } else {
        printf("out of memory, not receiving anymore\n");
    }
}

static void
_app_receive_error(void *conn, void *msg, char *reason)
{
    struct app_msg *m = msg;

    TAPS_TRACE();
    PRINT_RESULT("Receive failed", reason);
    free(m);
}

static void
_app_closed(void *conn)
{
    struct app_conn *c = conn;

    TAPS_TRACE();
    tapsConnectionFree(c->taps);
    free(c);
}

static void
_app_connection_error(void *conn, char *reason)
{
    struct app_conn *c = conn;

    TAPS_TRACE();
    tapsConnectionFree(c->taps);
    free(c);
}

static void *
_app_connection_received(void *listener, TAPS_CTX *conn, void **cb)
{
    struct app_listener  *l = listener;
    struct app_conn      *c = malloc(sizeof(struct app_conn));
    struct app_msg       *m;

    TAPS_TRACE();
    if (!c) {
        printf("malloc failed!\n");
        exit -1;
    }
    c->l = l;
    c->taps = conn;
    *cb = (void *)&l->callbacks;
    m = malloc(sizeof(struct app_msg));
    if (!m) {
        printf("malloc failed, not receiving anymore\n");
        exit(-1);
    }
    m->taps = tapsMessageNew(m->buf, BUF_SIZE);
    if (m->taps) {
        tapsConnectionReceive(c->taps, m, m->taps, MIN_BUF, BUF_SIZE,
                &c->l->callbacks);
    } else {
        printf("out of memory not receiving\n");
    }
    return c;
}

static void
_app_establishment_error(void *listener, char *reason)
{
    struct app_listener  *l = listener;

    TAPS_TRACE();
    PRINT_RESULT("Listener failed", reason);
    if (tapsListenerFree(l->taps) < 0) {
        printf("Listener free failed.\n");
    }
    free(l);
}

static void
_app_stopped(void *listener)
{
    struct app_listener  *l = listener;

    TAPS_TRACE();
    event_base_loopbreak(l->base);
}

static void
_app_sighandler(evutil_socket_t fd, short events, void *arg)
{
    struct app_listener *l = arg;

    TAPS_TRACE();
    tapsListenerStop(l->taps, &l->callbacks);
    printf("killing server\n");
}

/* We could assign callbacks on the fly, but since these are totally invariant
   we'll initialize them all here */
const tapsCallbacks cbs = {
    .connectionReceived = &_app_connection_received,
    .establishmentError = &_app_establishment_error,
    .stopped            = &_app_stopped,
    .sent               = &_app_sent,
    .expired            = &_app_expired,
    .sendError          = &_app_send_error,
    .received           = &_app_received,
    .receivedPartial    = &_app_received_partial,
    .receiveError       = &_app_receive_error,
    .closed             = &_app_closed,
    .connectionError    = &_app_connection_error,
};

/* This program is entirely sychronize, except handling the interrupt
   signal. A client written this way could be entirely synchronous. */
int
main(int argc, char *argv[])
{
    TAPS_CTX            *ep = NULL, *tp = NULL, *pc = NULL;
    struct event        *sigint = NULL;
    /* These have app-layer contexts. We could have just used the TAPS context
       if the relevant state (buf, base) were global variables, but this is
       cleaner. */
    struct app_listener *l = NULL;
    struct app_conn     *c = NULL;

    /* Configure endpoint */
    ep = tapsEndpointNew();
    if (!ep) {
        printf("Endpoint failed\n");
        goto fail;
    }
    if (!tapsEndpointWithPort(ep, 5555)) {
        printf("Port failed\n");
        goto fail;
    }
    if (!tapsEndpointWithIPv4Address(ep, "127.0.0.1")) {
        printf("IP Address failed\n");
        goto fail;
    }

    /* Set properties */
    tp = tapsTransportPropertiesNew(TAPS_LISTENER);
    if (!tp) {
        printf("Transport Properties failed\n");
        goto fail;
    }

    /* Start connection */
    pc = tapsPreconnectionNew(&ep, 1, NULL, 0, tp, NULL);
    if (!pc) {
        printf("Preconnection failed\n");
        goto fail;
    }
    l = malloc(sizeof(struct app_listener));
    if (!l) {
        goto fail;
    }
    l->taps = NULL;
    l->base = event_base_new();
    if (!l->base) {
        goto fail;
    }
    memcpy(&l->callbacks, &cbs, sizeof(cbs));
    /* Treat close and abort the same way */
    l->taps = tapsPreconnectionListen(pc, l, l->base, &l->callbacks);
    if (!l->taps) {
        printf("Listen failed\n");
        goto fail;
    }
    /* Clean up */
    tapsPreconnectionFree(pc);
    tapsEndpointFree(ep);
    tapsTransportPropertiesFree(tp);
    pc = NULL;
    ep = NULL;
    tp = NULL;

    sigint = event_new(l->base, SIGINT, EV_SIGNAL, _app_sighandler, l);
    event_add(sigint, NULL);
    event_base_dispatch(l->base);

    if (tapsListenerFree(l->taps) < 0) {
        printf("Listener free failed.\n");
    }
    event_del(sigint);
    event_free(sigint);
    event_base_free(l->base);
    free(l);
    return 0;
fail:
    if (pc) tapsPreconnectionFree(pc);
    if (ep) tapsEndpointFree(ep);
    if (tp) tapsTransportPropertiesFree(tp);
    if (sigint) event_free(sigint);
    if (l) {
        if (l->taps) tapsListenerFree(l->taps);
        if (l->base) event_base_free(l->base);
        free(l);
    }
}
