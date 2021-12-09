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

#include <dlfcn.h>
#include <event2/event.h>
#include <event2/util.h>
#include <stdlib.h>
#include <string.h>
#include "taps_internals.h"

typedef struct {
    void                    *proto_ctx; /* Opaque blob used by the protocol */
    void                    *app_ctx; /* Opaque blob used by the app */
    struct proto_handles     handles; /* Location of protocol functions */
    tapsCbConnectionReceived connectionReceived;
    tapsCbEstablishmentError establishmentError;
    tapsCbStopped            stopped;
    uint32_t                 ref_count;
    uint32_t                 conn_limit;
    struct event_base       *base;
    int                      readyToStop;
    int                      baseCreatedHere; /* Did TAPS init the event_base?*/
} tapsListener;

static void *
_taps_connection_received(void *taps_ctx, void *proto_ctx)
{
    tapsListener   *l = taps_ctx;
    tapsCallbacks  *callbacks;
    TAPS_CTX       *c;
    void           *app_ctx;

    TAPS_TRACE();
    c = tapsConnectionNew(proto_ctx, &l->handles, l);
    if (!c) {
        printf("tapsConnectionNew failed\n");
        return NULL;
    }
    l->ref_count++;
    app_ctx = (*(l->connectionReceived))(l->app_ctx, c, (void **)&callbacks);
    if (!callbacks || !callbacks->closed || !callbacks->connectionError) {
        _taps_closed(c);
        printf("connectionReceived callback did not return callbacks\n");
        return NULL;
    }
    tapsConnectionInitialize(c, app_ctx, callbacks);
    return c;
}

static void
_taps_stopped(void *taps_ctx)
{
    tapsListener *l = taps_ctx;
    tapsCbStopped stopped = l->stopped;

    TAPS_TRACE();
    l->readyToStop = TRUE;
    if (l->ref_count == 0) {
        l->stopped = NULL; /* Mark this as dead */
        (*stopped)(l->app_ctx);
    }
}

TAPS_CTX *
tapsListenerNew(void *app_ctx, struct proto_handles *handles,
        struct sockaddr *addr, struct event_base *base,
        tapsCallbacks *callbacks)
{
    tapsListener      *l;

    TAPS_TRACE();
    l = malloc(sizeof(tapsListener));
    if (!l) {
        errno = ENOMEM;
        return l;
    }
    memset(l, 0, sizeof(tapsListener));
    memcpy(&l->handles, handles, sizeof(struct proto_handles));
    l->baseCreatedHere = (base == NULL);
    l->base = l->baseCreatedHere ? event_base_new() : base;
    if (!l->base) {
       printf("base failed\n");
       goto fail;
    }
    /* XXX fix this */
    if (l->baseCreatedHere) {
        printf("Error: TAPS currently requires the application to provide an "
                "event_base\n");
        goto fail;
    }
    /* It would be good to get rid of doing the callbacks here */
    l->proto_ctx = (l->handles.listen)(l, l->base, addr,
            &_taps_connection_received, NULL,
            &_taps_closed, &_taps_connection_error);
    if (!l->proto_ctx) {
        printf("Protocol Listen failed\n");
        goto fail;
        /* XXX early failure */
    }
    l->conn_limit = UINT32_MAX;
    l->connectionReceived = callbacks->connectionReceived;
    l->establishmentError = callbacks->establishmentError;
    l->app_ctx = app_ctx ? app_ctx : l;
    return l;
fail:
    if (l) {
        if (l->handles.proto) dlclose(l->handles.proto);
        if (l->base && l->baseCreatedHere) event_base_free(l->base);
        free(l);
    }
    return NULL;
}

int
tapsListenerStop(TAPS_CTX *listener, tapsCallbacks *callbacks)
{
    tapsListener     *l = (tapsListener *)listener;

    TAPS_TRACE();
    if (!listener || !callbacks || !callbacks->stopped) {
        errno = EINVAL;
        return -1;
    }
    l->stopped = callbacks->stopped;
    (l->handles.stop)(l->proto_ctx, &_taps_stopped);
    return 0;
}

void
tapsListenerDeref(TAPS_CTX *listener)
{
    tapsListener *l = (tapsListener *)listener;
    tapsCbStopped stopped = l->stopped;

    l->ref_count--;
    if (l->readyToStop && (l->ref_count == 0)) {
        l->stopped = NULL;
        (*stopped)(l->app_ctx);
    }
}

int
tapsListenerFree(TAPS_CTX *listener)
{
    tapsListener     *l = (tapsListener *)listener;

    TAPS_TRACE();
    if (!l->readyToStop && !l->stopped) {
        /* Early Free */
        (l->handles.stop)(l->proto_ctx, &_taps_stopped);
        l->readyToStop = TRUE;
        return 0;
    }
    if (l->stopped) {
        printf("Trying to free before stopping\n");
        return -1;
    }
    dlclose(l->handles.proto); /* XXX check for errors */
    free(l);
    return 0;
}
