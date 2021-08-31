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
void
sendRecvError(TAPS_CTX *ctx, void *data, size_t data_len)
{
    printf("send or receive failed\n");
}

void
sendExpired(TAPS_CTX *ctx, void *data, size_t data_len)
{
    printf("send expired\n");
}

void
connSent(TAPS_CTX *ctx, void *data, size_t data_len)
{
    printf("response sent\n");
}

void
connRcv(TAPS_CTX *ctx, void *data, size_t data_len)
{
    printf("echoing %lu bytes\n", data_len);
//    tapsConnectionSend(ctx, data, data_len, NULL, FALSE, &connSent,
//            &sendExpired, &sendRecvError);
    /* Get ready for more! */
//    tapsConnectionReceive(conn, 0, 0, &connRcv, &connRcv, &sendRecvError);
}

void
connClose(TAPS_CTX *ctx, void *data, size_t data_len)
{
    printf("Client closed connection\n");
//    tapsConnectionFree(ctx);
}

void
listenerConnRcvd(TAPS_CTX *ctx, void *data, size_t data_len)
{
    //TAPS_CTX *conn = data;
    /* Do the same thing for "partial" and full inputs */
//    tapsConnectionReceive(conn, 0, 0, &connRcv, &connRcv, &sendRecvError);
    return;
}

void
listenerError(TAPS_CTX *ctx, void *data, size_t data_len)
{
    printf("Listener returned error\n");
    tapsListenerFree(ctx);
    if (tapsListenerFree(ctx) < 0) {
        printf("Listener free failed.\n");
    }
}

void
listenerStopped(TAPS_CTX *ctx, void *data, size_t data_len)
{
    printf("Listener Stopped\n");
    if (tapsListenerFree(ctx) < 0) {
        printf("Listener free failed.\n");
    }
    event_base_loopbreak(base);
}

static void sighandler(evutil_socket_t fd, short events, void *arg)
{
    TAPS_CTX *listener = arg;

    tapsListenerStop(listener, &listenerStopped);
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
    listener = tapsPreconnectionListen(pc, base, &listenerConnRcvd, &listenerError);
    if (!listener) {
        printf("Listen failed\n");
        return -1;
    }
    /* Clean up */
    tapsPreconnectionFree(pc);
    tapsEndpointFree(ep);
    tapsTransportPropertiesFree(tp);

    sigint = event_new(base, SIGINT, EV_SIGNAL, sighandler, listener);
    event_add(sigint, NULL);
    event_base_dispatch(base);

    event_del(sigint);
    event_free(sigint);
    event_base_free(base);
    return 1;
}
