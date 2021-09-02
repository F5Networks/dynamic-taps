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

#include <stdlib.h>
#include "taps_internals.h"

typedef enum { TAPS_START, TAPS_HAVEADDR, TAPS_CONNECTING, TAPS_CONNECTED }
        tapsCandidateState;

typedef struct _tapsConnection {
    void                 *proto_ctx; /* socket descriptor, openSSL ctx, etc. */
    struct proto_handles *handles;
    tapsCandidateState    state;
    tapsCallback          sent;
    tapsCallback          expired;
    tapsCallback          sendError;
    tapsCallback          received;
    tapsCallback          receivedPartial;
    tapsCallback          receiveError;
    tapsCallback          closed;
    tapsCallback          connectionError;
    //char               *localIf;
    //struct sockaddr    *remote;
    TAPS_CTX             *listener; /* NULL for Initiated connections */
} tapsConnection;

void
_taps_closed(void *taps_ctx)
{
    tapsConnection *c = taps_ctx;

    TAPS_TRACE();
    if (c->listener) {
        tapsListenerDeref(c->listener);
        c->listener = NULL;
    } else {
        free(c->handles);
    }
    (c->closed)(taps_ctx, NULL, 0);
}

void
_taps_connection_error(void *taps_ctx)
{
    tapsConnection *c = taps_ctx;

    TAPS_TRACE();
    if (c->listener) {
        tapsListenerDeref(c->listener);
        c->listener = NULL;
    } else {
        free(c->handles);
    }
    (c->connectionError)(taps_ctx, NULL, 0);
}

TAPS_CTX *
tapsConnectionNew(void *proto_ctx, struct proto_handles *handles,
        TAPS_CTX *listener, tapsCallback closed, tapsCallback connectionError)
{
    tapsConnection *c = malloc(sizeof(tapsConnection));

    TAPS_TRACE();
    if (!c) return c;
    c->proto_ctx = proto_ctx;
    c->handles = handles;
    c->state = TAPS_CONNECTED;
    c->closed = closed;
    c->connectionError = connectionError;
    c->listener = listener;
    return c;
}

void
tapsConnectionSend(TAPS_CTX *connection, void *data, size_t data_len,
        tapsCallback *sent, tapsCallback *expired, tapsCallback *error)
{
    tapsConnection *conn = (tapsConnection *)connection;

//    (conn->listener->sendHandle)(data, data_len);
}

void
tapsConnectionReceive(TAPS_CTX *connection, size_t minIncompleteLength,
        size_t maxLength, tapsCallback *received,
        tapsCallback *receivedPartial, tapsCallback *error)
{
//    (conn->listener->recvHandle)();
}

void
tapsConnectionFree(TAPS_CTX *connection)
{
    TAPS_TRACE();
    /* If no listener, we should free the protocol handle */
    free(connection);
}
