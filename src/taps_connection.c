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
#include "taps_protocol.h"
#include "taps.h"

/* Callbacks */
static void
_taps_closed(void *ctx)
{
    tapsConnection *conn = ctx;
#if 0
    (conn->close)((TAPS_CTX *)ctx, NULL, 0);
    conn->context = NULL;
    conn->protoHandle = NULL;
    _taps_listener_deref(conn->listener);
#endif
}

static void
_taps_connection_error(void *ctx)
{
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
    free(connection);
}
