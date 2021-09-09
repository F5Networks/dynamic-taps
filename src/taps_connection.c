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

#define MAX_RECVBUF_SIZE 65536

typedef enum { TAPS_START, TAPS_HAVEADDR, TAPS_CONNECTING, TAPS_CONNECTED }
        tapsCandidateState;

struct _send_item {
    void                   *message;
    TAPS_CTX               *connection;
    tapsCallback            sent;
    tapsCallback            expired;
    tapsCallback            sendError;
    struct _send_item      *next, *prev;
};

struct _recv_item {
    void                   *data;
    size_t                  minLength, maxLength, currLength;
    TAPS_CTX               *connection;
    tapsCallback            received;
    tapsCallback            receivedPartial;
    tapsCallback            receiveError;
    struct _recv_item      *next, *prev;
};

#define ADD_ITEM(type, tail)                                 \
    struct type *newItem = malloc(sizeof(struct type));      \
    if (newItem) {                                           \
        newItem->next = NULL;                                \
        newItem->prev = (tail);                              \
        if (tail) {                                          \
            (tail)->next = newItem;                          \
        }                                                    \
        (tail) = newItem;                                    \
    }                                                        \

#define DELETE_ITEM(item, tail)                \
    if ((item)->prev) {                        \
        (item)->prev->next = (item)->next;     \
    }                                          \
    if ((item)->next) {                        \
        (item)->next->prev = (item)->prev;     \
    } else {                                   \
        (tail) = (item)->prev;                 \
    }                                          \
    free(item);

typedef struct {
    void                   *proto_ctx; /* socket, openSSL ctx, etc. */
    struct proto_handles   *handles;
    tapsCandidateState      state;
    struct _send_item      *sndq; /* Pts to tail of list */
    struct _recv_item      *rcvq; /* Pts to tail of list */
    tapsCallback            closed;
    tapsCallback            connectionError;
    /* Only send one send or receive command to protocol at a time */
    int                     sendReady;
    int                     receiveReady;
    //char                 *localIf;
    //struct sockaddr      *remote;
    TAPS_CTX               *listener; /* NULL for Initiated connections */
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
    c->sndq = NULL;
    c->rcvq = NULL;
    c->closed = closed;
    c->connectionError = connectionError;
    c->listener = listener;
    c->sendReady = TRUE;
    c->receiveReady = TRUE;
    return c;
}

static void
_taps_sent(TAPS_CTX *conn, TAPS_CTX *msg)
{

}

static void
_taps_expired(TAPS_CTX *conn, TAPS_CTX *msg)
{

}

static void
_taps_send_error(TAPS_CTX *conn, TAPS_CTX *msg)
{

}

void
tapsConnectionSend(TAPS_CTX *connection, TAPS_CTX *msg,
        tapsCallback sent, tapsCallback expired, tapsCallback sendError)
{
#if 0
    tapsConnection *c = (tapsConnection *)connection;
    struct _send_recv_item *newItem;

    newItem = add_item(&c->sndq);
    if (!newItem) {
        (sendError)(connection, msg, 0);
        return;
    }

    newItem->msg = msg;
    newItem->connection = connection;
    newItem->__sent = sent;
    newItem->__expired = expired;
    newItem->__sendError = sendError;

    if (c->sendReady) {
        (c->handles->send)(c->proto_ctx, newItem, msg, &_taps_sent,
                &_taps_expired, &_taps_send_error);
    }
#endif
}

static void
_taps_received(void *item_ctx, void *data, size_t data_len)
{
}

static void
_taps_receive_error(TAPS_CTX *item_ctx)
{

}

static void
_taps_received_partial(void *item_ctx, void *data, size_t data_len)
{
    struct _recv_item *item = item_ctx;
    struct _recv_item *next_item;
    tapsConnection    *c = item->connection;
    tapsCallback       fn;
    TAPS_CTX          *msg;
    size_t             len;

    item->currLength += data_len;
    /* Send it back if not enough length */
    if (item->currLength < item->minLength) {
        (c->handles->receive)(c->proto_ctx, item,
                item->data + item->currLength,
                item->maxLength - item->currLength , &_taps_received,
                &_taps_received_partial, &_taps_receive_error);
        return;
    }
    msg = tapsMessageNew(item->data, item->currLength);
    if (item->next) {
        /* Queue up the next receive */
        (c->handles->receive)(c->proto_ctx, item->next, item->next->data,
                item->next->maxLength,
                &_taps_received, &_taps_received_partial, &_taps_receive_error);
    } else {
        /* Nothing else to do */
        c->receiveReady = TRUE;
    }
    fn = item->receivedPartial;
    DELETE_ITEM(item, c->rcvq);
    (fn)(c, msg, 0);
}

void
tapsConnectionReceive(TAPS_CTX *connection, void *buf,
        size_t minIncompleteLength, size_t maxLength, tapsCallback received,
        tapsCallback receivedPartial, tapsCallback receiveError)
{
    tapsConnection *c = (tapsConnection *)connection;

    TAPS_TRACE();
    ADD_ITEM(_recv_item, c->rcvq);
    newItem->received = received;
    newItem->receivedPartial = receivedPartial;
    newItem->receiveError = receiveError;
    newItem->data = buf;
    newItem->minLength = minIncompleteLength;
    newItem->maxLength = maxLength;
    newItem->currLength = 0;
    newItem->connection = c;

    if (c->receiveReady) {
        c->receiveReady = FALSE;
        (c->handles->receive)(c->proto_ctx, newItem, newItem->data, maxLength,
                &_taps_received, &_taps_received_partial, &_taps_receive_error);
    }
}

void
tapsConnectionFree(TAPS_CTX *connection)
{
    TAPS_TRACE();
    /* If no listener, we should free the protocol handle */
    free(connection);
}
