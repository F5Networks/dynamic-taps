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
#include "../src/taps.h"

static struct event_base *base;

/* Callbacks */
static void
_app_send_error(TAPS_CTX *ctx, void *data, size_t data_len)
{
    TAPS_TRACE();
}

static void
_app_expired(TAPS_CTX *ctx, void *data, size_t data_len)
{
    TAPS_TRACE();
}

static void
_app_sent(TAPS_CTX *ctx, void *data, size_t data_len)
{
    TAPS_TRACE();
}

static void
_app_received(TAPS_CTX *ctx, void *data, size_t data_len)
{
    TAPS_TRACE();
//    tapsConnectionSend(ctx, data, data_len, NULL, FALSE, &appConnSent,
//            &appSendExpired, &appSendRecvError);
    /* Get ready for more! */
//    tapsConnectionReceive(conn, 0, 0, &appConnRcv, &appConnRcv,
   // &appSendRecvError);
}

static void
_app_closed(TAPS_CTX *ctx, void *data, size_t data_len)
{
    TAPS_TRACE();
//    tapsConnectionFree(ctx);
}

static void
_app_connection_received(TAPS_CTX *ctx, void *data, size_t data_len)
{
    //TAPS_CTX *conn = data;
    TAPS_TRACE();
    /* Do the same thing for "partial" and full inputs */
//    tapsConnectionReceive(conn, 0, 0, &connRcv, &connRcv, &sendRecvError);
    return;
}

static void
_app_establishment_error(TAPS_CTX *ctx, void *data, size_t data_len)
{
    TAPS_TRACE();
    if (tapsListenerFree(ctx) < 0) {
        printf("Listener free failed.\n");
    }
}

static void
_app_stopped(TAPS_CTX *ctx, void *data, size_t data_len)
{
    TAPS_TRACE();
    event_base_loopbreak(base);
}

static void
_app_sighandler(evutil_socket_t fd, short events, void *arg)
{
    TAPS_CTX *listener = arg;

    TAPS_TRACE();
    tapsListenerStop(listener, &_app_stopped);
    printf("killing server\n");
}

/* This program is entirely sychronize, except handling the interrupt
   signal. A client written this way could be entirely synchronous. */
int
main(int argc, char *argv[])
{
    TAPS_CTX *ep, *tp, *pc, *listener;
    struct event *sigint;

    base = event_base_new();
    /* Configure endpoint */
    ep = tapsEndpointNew();
    if (!ep) {
        printf("Endpoint failed\n");
        return -1;
    }
    if (!tapsEndpointWithPort(ep, 5555)) {
        printf("Port failed\n");
        return -1;
    }
    if (!tapsEndpointWithIPv4Address(ep, "127.0.0.1")) {
        printf("IP Address failed\n");
        return -1;
    }

    /* Set properties */
    tp = tapsTransportPropertiesNew(TAPS_LISTENER);
    if (!tp) {
        printf("Transport Properties failed\n");
        return -1;
    }

    /* Start connection */
    pc = tapsPreconnectionNew(&ep, 1, NULL, 0, tp, NULL);
    if (!pc) {
        printf("Preconnection failed\n");
        return -1;
    }
    /* Treat close and abort the same way */
    listener = tapsPreconnectionListen(pc, base, &_app_connection_received, 
            &_app_establishment_error, &_app_closed, &_app_closed);
    if (!listener) {
        printf("Listen failed\n");
        return -1;
    }
    /* Clean up */
    tapsPreconnectionFree(pc);
    tapsEndpointFree(ep);
    tapsTransportPropertiesFree(tp);

    sigint = event_new(base, SIGINT, EV_SIGNAL, _app_sighandler, listener);
    event_add(sigint, NULL);
    event_base_dispatch(base);

    if (tapsListenerFree(listener) < 0) {
        printf("Listener free failed.\n");
    }
    event_del(sigint);
    event_free(sigint);
    event_base_free(base);
    return 1;
}
