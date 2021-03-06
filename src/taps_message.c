
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
#include <sys/uio.h>
#include "taps_internals.h"

#if 0
typedef struct {
    tapsFramer             *sendFramer, *recvFramer;
    tapsEndpoint           *remoteEndpoint, *localEndpoint;
    unsigned int            msgLifetime; /* 0 = infinite */
    unsigned int            msgPrio;
    bool                    msgOrdered;
    bool                    safelyReplayable;
    bool                    final;
    int                     msgChecksumLen; /* -1 = full coverage */
    bool                    msgReliable;
    tapsCapacity            msgCapacityProfile;
    bool                    noFragmentation, noSegmentation;
} tapsMessageContext;

static const tapsMessageContext tapsMessageDefault = {
    .sendFramer            = NULL,
    .recvFramer            = NULL,
    .msgLifetime           = 0,
    .msgPrio               = 100,
    /* Skip msgOrdered */
    .safelyReplayable      = false,
    .final                 = false,
    .msgChecksumLen        = -1,
    /* Skip msgReliable, msgCapacityProfile */
    .noFragmentation       = false,
    .noSegmentation        = false,
};
#endif

typedef struct {
} tapsMessageProperties;

typedef struct {
    struct iovec          buf;  /* Only used if iovcnt == 1 */
    struct iovec         *list; /* NULL unless iovcnt > 1 */
    int                   iovcnt; /* If iovcnt = 1, it's just a buffer */
    tapsMessageProperties props;
} tapsMessage;

TAPS_CTX *
tapsMessageNew(void *data, size_t len)
{
    tapsMessage *m = malloc(sizeof(tapsMessage));

    TAPS_TRACE();
    if (!m) {
        errno = ENOMEM;
        return NULL;
    }
    m->buf.iov_base = data;
    m->buf.iov_len = len;
    m->list = NULL;
    m->iovcnt = 1;
    memset(&m->props, 0, sizeof(tapsMessageProperties));
    return m;
}

void *
tapsMessageGetFirstBuf(TAPS_CTX *message, size_t *len)
{
    tapsMessage *m = (tapsMessage *)message;
    struct iovec *buf = (m->list) ? m->list : &(m->buf);

    TAPS_TRACE();
    *len = buf->iov_len;
    return buf->iov_base;
}

struct iovec *
tapsMessageGetIovec(TAPS_CTX *message, int *iovcnt)
{
    tapsMessage *m = (tapsMessage *)message;

    if (iovcnt) *iovcnt = m->iovcnt;
    return (m->list ? m->list : &(m->buf));
}

void
tapsMessageTruncate(TAPS_CTX *message, size_t length)
{
    tapsMessage  *m = (tapsMessage *)message;
    struct iovec *buf = (m->list) ? m->list : &(m->buf);
    size_t        remaining = length;
    int           row = 0;

    while ((remaining > buf->iov_len) && (row < m->iovcnt)) {
        remaining -= buf->iov_len;
        buf++;
        row++;
    }
    if (row == m->iovcnt) {
        m->iovcnt = 0;
        return;
    }
    m->iovcnt = row + 1;
    buf->iov_len = remaining;
}

void
tapsMessageFree(TAPS_CTX *message)
{
    tapsMessage *m = (tapsMessage *)message;
    struct iovec *buf = (m->list) ? m->list : &(m->buf);

    TAPS_TRACE();
#if 0 /* buf->iov_base comes from the app (?); don't free */
    while (m->iovcnt) {
        free(buf->iov_base);
        buf++;
        m->iovcnt--;
    }
#endif
    if (m->list) free(m->list);
    free(m);
}
