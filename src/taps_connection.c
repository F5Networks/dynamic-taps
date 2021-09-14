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
#include <string.h>
#include "taps_internals.h"

#define MAX_RECVBUF_SIZE 65536

typedef enum { TAPS_START, TAPS_HAVEADDR, TAPS_CONNECTING, TAPS_CONNECTED }
        tapsCandidateState;

struct _send_item {
    TAPS_CTX               *message;
    TAPS_CTX               *connection;
    void                   *app_ctx;
    tapsCbSent              sent;
    tapsCbExpired           expired;
    tapsCbSendError         sendError;
    struct _send_item      *next, *prev;
};

struct _recv_item {
    TAPS_CTX               *message;
    size_t                  minLength, maxLength, currLength;
    TAPS_CTX               *connection;
    void                   *app_ctx;
    tapsCbReceived          received;
    tapsCbReceivedPartial   receivedPartial;
    tapsCbReceiveError      receiveError;
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
        (*tail) = (item)->prev;                 \
    }                                          \
    free(item);

typedef struct {
    void                   *proto_ctx; /* socket, openSSL ctx, etc. */
    void                   *app_ctx;
    struct proto_handles   *handles; /* Symbols for dynamic functions */
    tapsCandidateState      state;
    struct _send_item      *sndq; /* Pts to tail of list */
    struct _recv_item      *rcvq; /* Pts to tail of list */
    tapsCbClosed            closed;
    tapsCbConnectionError   connectionError;
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
    c->proto_ctx = NULL;
    if (c->closed) (c->closed)(c->app_ctx);
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
    (c->connectionError)(c->app_ctx, NULL);
}

TAPS_CTX *
tapsConnectionNew(void *proto_ctx, struct proto_handles *handles,
        TAPS_CTX *listener)
{
    tapsConnection *c = malloc(sizeof(tapsConnection));

    TAPS_TRACE();
    if (!c) return c;
    memset(c, 0, sizeof(tapsConnection));
    c->proto_ctx = proto_ctx;
    c->handles = handles;
    c->state = TAPS_CONNECTED;
    c->listener = listener;
    c->sendReady = TRUE;
    c->receiveReady = TRUE;
    return c;
}

void
tapsConnectionInitialize(TAPS_CTX *conn, void *app_ctx, 
        tapsCallbacks *callbacks)
{
    tapsConnection *c = conn;

    TAPS_TRACE();
    c->app_ctx = app_ctx ? app_ctx : c;
    c->closed = callbacks->closed;
    c->connectionError = callbacks->connectionError;
}

void _taps_sent(void *item_ctx);
void _taps_expired(void *item_ctx);
void _taps_send_error(void *item_ctx, char *reason);

static int
_taps_send_common(tapsConnection *c, struct _send_item *item)
{
    struct _send_item *next = item->next;
    struct iovec *data;
    int           iovcnt;

    DELETE_ITEM(item, &(c->sndq));
    if (!next) {
        c->sendReady = TRUE;
        return 0;
    }
    data = tapsMessageGetIovec(next->message, &iovcnt);
    return (c->handles->send)(c->proto_ctx, next, data, iovcnt, &_taps_sent,
            &_taps_expired, &_taps_send_error);
}

void
_taps_sent(void *item_ctx)
{
    struct _send_item *item = item_ctx;
    tapsConnection    *c = item->connection;
    tapsCbSent         cb = item->sent;
    void              *app = item->app_ctx;

    TAPS_TRACE();
    printf("msg %p\n", item->message);
    if (_taps_send_common(c, item) < 0) {
        printf("send failed\n");
    }
    (cb)(c->app_ctx, app);
}

void
_taps_expired(void *item_ctx)
{
    struct _send_item *item = item_ctx;
    tapsConnection    *c = item->connection;
    tapsCbExpired      cb = item->expired;
    void              *app = item->app_ctx;

    TAPS_TRACE();
    if (_taps_send_common(c, item) < 0) {
        printf("send failed\n");
    }
    (cb)(c->app_ctx, app);
}

void
_taps_send_error(void *item_ctx, char *reason)
{
    struct _send_item *item = item_ctx;
    tapsConnection    *c = item->connection;
    tapsCbSendError    cb = item->sendError;
    void              *app = item->app_ctx;

    TAPS_TRACE();
    if (_taps_send_common(c, item) < 0) {
        printf("send failed\n");
    }
    (cb)(c->app_ctx, app, (reason) ? reason : "Protocol failure");
}

int
tapsConnectionSend(TAPS_CTX *connection, TAPS_CTX *msg, void *app_ctx,
        tapsCallbacks *callbacks)
{
    tapsConnection *c = (tapsConnection *)connection;
    struct iovec *data;
    int iovlen;

    printf("Send: setting %p\n", msg);
    ADD_ITEM(_send_item, c->sndq);
    if (!newItem) {
        errno = ENOMEM;
        printf("Sending failed\n");
        return -1;
    }
    printf("Send: setting %p\n", msg);
    newItem->message = msg;
    newItem->connection = connection;
    newItem->app_ctx = app_ctx;
    newItem->sent = callbacks->sent;
    newItem->expired = callbacks->expired;
    newItem->sendError = callbacks->sendError;

    if (c->sendReady) {
        data = tapsMessageGetIovec(msg, &iovlen);
        return (c->handles->send)(c->proto_ctx, newItem, data, iovlen,
                &_taps_sent, &_taps_expired, &_taps_send_error);
    }
    return 0;
}

/* Creates a new iovec to points to the same buffers as the original, but
   leaving off the first len bytes. */
static struct iovec *
_taps_trunc_iovec(struct iovec *orig, int *iovcnt, size_t len)
{
    size_t        remaining = len;
    struct iovec *ptr = orig;
    struct iovec *newVec;

    while ((remaining > ptr->iov_len) && (iovcnt > 0)) {
        (*iovcnt)--;
        remaining -= ptr->iov_len;
        ptr++;
    }
    if (iovcnt == 0) {
        printf("Iovec not big enough to truncate!\n");
        return NULL;
    }
    newVec = malloc(sizeof(struct iovec) * (*iovcnt));
    if (!newVec) {
        return newVec;
    }
    memcpy(newVec, ptr, sizeof(struct iovec) * (*iovcnt));
    newVec->iov_base += remaining;
    return newVec;
}

static void
_taps_receive_error(TAPS_CTX *item_ctx, struct iovec *data, char *reason)
{
    struct _recv_item     *item = item_ctx;
    struct iovec          *iovec = tapsMessageGetIovec(item->message, NULL);
    tapsCbReceiveError     fn = item->receiveError;
    tapsConnection        *c = item->connection;
    TAPS_CTX              *conn_ctx = c->app_ctx;
    TAPS_CTX              *item_app = item->app_ctx;

    if (iovec != data) {
        free(data);
    }
    DELETE_ITEM(item, &(c->rcvq));
    (fn)(conn_ctx, item_app, reason);
}

static void
_taps_received(void *item_ctx, struct iovec *data, size_t data_len)
{
    struct _recv_item     *item = item_ctx;
    struct iovec          *iovec = tapsMessageGetIovec(item->message, NULL);
    tapsCbReceived         fn = item->received;
    tapsConnection        *c = item->connection;
    TAPS_CTX              *conn_ctx = c->app_ctx;
    TAPS_CTX              *item_app = item->app_ctx;
    TAPS_CTX              *msg = item->message;

    if (iovec != data) {
        free(data);
    }
    item->currLength += data_len;
    if (item->currLength < item->minLength) {
        /* If the message size is < minLength, it is an error */
        _taps_receive_error(item_ctx, iovec, "Message below minLength");
        return;
    }
    DELETE_ITEM(item, &(c->rcvq));
    (fn)(conn_ctx, item_app);
}

static void
_taps_received_partial(void *item_ctx, struct iovec *data, size_t data_len)
{
    struct _recv_item     *item = item_ctx;
    struct _recv_item     *next_item;
    tapsConnection        *c = item->connection;
    tapsCbReceivedPartial  rcvPart;
    TAPS_CTX              *msg;
    void                  *app_ctx;
    size_t                 len;
    struct iovec          *iovec, *newIovec;
    int                    iovcnt;

    iovec = tapsMessageGetIovec(item->message, &iovcnt);
    /* Handle returns below minLength. Unfortunately, the SO_RCVLOWAT socket
       option doesn't work with select() and poll(), which in turn are wrapped
       by libevent. So we have to handle this manually.

       We have to manually hack the iovec to make this work.*/
    if (iovec != data) {
        free(data);
    }
    item->currLength += data_len;
    if (item->currLength < item->minLength) {
        newIovec = _taps_trunc_iovec(iovec, &iovcnt, item->currLength);
        if (!newIovec) {
            _taps_receive_error(item_ctx, iovec, "Internal error");
            return;
        }
        (c->handles->receive)(c->proto_ctx, item, newIovec, iovcnt,
                &_taps_received, &_taps_received_partial, &_taps_receive_error);
        return;
    }
    if (item->next) {
        /* Queue up the next receive */
        iovec = tapsMessageGetIovec(item->next->message, &iovcnt);
        (c->handles->receive)(c->proto_ctx, item->next, iovec, iovcnt,
                &_taps_received, &_taps_received_partial, &_taps_receive_error);
    } else {
        /* Nothing else to do */
        c->receiveReady = TRUE;
    }
    rcvPart = item->receivedPartial;
    app_ctx = item->app_ctx;
    msg = item->message;
    DELETE_ITEM(item, &(c->rcvq));
    (rcvPart)(c->app_ctx, app_ctx, FALSE);
}

int
tapsConnectionReceive(TAPS_CTX *connection, void *app_ctx, TAPS_CTX *msg,
    size_t minIncompleteLength, size_t maxLength, tapsCallbacks *callbacks)
{
    tapsConnection *c = (tapsConnection *)connection;
    struct iovec   *iovec;
    int             iovcnt;

    TAPS_TRACE();
    if (!callbacks || !callbacks->received || !callbacks->receivedPartial ||
            !callbacks->receiveError) {
        printf("Not enough callbacks");
        errno = EINVAL;
        return -1;
    }
    ADD_ITEM(_recv_item, c->rcvq);
    newItem->received = callbacks->received;
    newItem->receivedPartial = callbacks->receivedPartial;
    newItem->receiveError = callbacks->receiveError;
    newItem->message = msg;
    newItem->minLength = minIncompleteLength;
    newItem->maxLength = maxLength;
    newItem->currLength = 0;
    newItem->connection = c;
    newItem->app_ctx = app_ctx;

    if (c->receiveReady) {
        c->receiveReady = FALSE;
        iovec = tapsMessageGetIovec(msg, &iovcnt);
        (c->handles->receive)(c->proto_ctx, newItem, iovec, iovcnt,
        &_taps_received, &_taps_received_partial,
               &_taps_receive_error);
    }
    return 0;
}

void
tapsConnectionFree(TAPS_CTX *connection)
{
    tapsConnection *c = connection;

    TAPS_TRACE();
    while (c->sndq) {
        (c->sndq->sendError)(c->app_ctx, c->sndq->app_ctx, "Connection died");
        DELETE_ITEM(c->sndq, &(c->sndq));
    }
    while (c->rcvq) {
        (c->rcvq->receiveError)(c->app_ctx, c->rcvq->app_ctx,
                "Connection died");
        DELETE_ITEM(c->rcvq, &(c->rcvq));
    }

    /* If no listener, we should free the protocol handle */
    free(connection);
}
