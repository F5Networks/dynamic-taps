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
    void                *proto_ctx; /* Opaque blob for use by the protocol */
    struct proto_handles handles;
    tapsCallback         connectionReceived;
    tapsCallback         establishmentError;
    tapsCallback         stopped;
    tapsCallback         closed; /* Listener only uses to init connection */
    tapsCallback         connectionError; /* only for connection */
    uint32_t             ref_count;
    uint32_t             conn_limit;
    struct event_base   *base;
    int                  readyToStop;
    int                  baseCreatedHere; /* Did the app provide the base? */
} tapsListener;

static void *
_taps_connection_received(void *taps_ctx, void *proto_ctx)
{
    tapsListener   *l = taps_ctx;
    TAPS_CTX       *conn;

    TAPS_TRACE();
    conn = tapsConnectionNew(proto_ctx, &l->handles, l, l->closed,
            l->connectionError);
    if (conn == NULL) {
        printf("tapsConnectionNew failed\n");
        return NULL;
    }
    l->ref_count++;
    (*(l->connectionReceived))(l, conn, sizeof(TAPS_CTX *));
    return conn;
}

static void
_taps_stopped(void *taps_ctx)
{
    tapsListener *l = taps_ctx;
    tapsCallback  stopped = l->stopped;

    TAPS_TRACE();
    if (!l->stopped) {
        printf("We already stopped\n");
        return; /* Can't stop twice! */
    }
    l->readyToStop = TRUE;
    if (l->ref_count == 0) {
        l->stopped = NULL; /* Mark this as dead */
        (*stopped)((TAPS_CTX *)taps_ctx, NULL, 0);
    }
}

TAPS_CTX *
tapsListenerNew(char *libpath, struct sockaddr *addr, struct event_base *base,
        tapsCallback connectionReceived, tapsCallback establishmentError,
        tapsCallback closed, tapsCallback connectionError)
{
    tapsListener      *l;

    TAPS_TRACE();
    l = malloc(sizeof(tapsListener));
    if (!l) {
        errno = ENOMEM;
        return l;
    }
    memset(l, 0, sizeof(tapsListener));
    l->handles.proto = dlopen(libpath, RTLD_LAZY);
    if (!l->handles.proto) {
        printf("Couldn't get protocol handle: %s\n", dlerror());
        goto fail;
    }
    l->handles.listen = dlsym(l->handles.proto, "Listen");
    if (!l->handles.listen) {
        printf("Couldn't get Listen handle: %s\n", dlerror());
        goto fail;
    }
    /* Fail fast if the protocol does not have all the required handles.
       This vetting could occur in tapsd? The preconnection? */
    l->handles.stop = dlsym(l->handles.proto, "Stop");
    if (!l->handles.stop) {
        printf("Couldn't get Stop handle: %s\n", dlerror());
        goto fail;
    }
    l->handles.send = dlsym(l->handles.proto, "Send");
    if (!l->handles.send) {
        printf("Couldn't get Send handle: %s\n", dlerror());
        goto fail;
    }
    l->handles.receive = dlsym(l->handles.proto, "Receive");
    if (!l->handles.receive) {
        printf("Couldn't get Receive handle: %s\n", dlerror());
        goto fail;
    }
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
    l->proto_ctx = (l->handles.listen)(l, l->base, addr,
            &_taps_connection_received, NULL,
            &_taps_closed, &_taps_connection_error);
    if (!l->proto_ctx) {
        printf("Protocol Listen failed\n");
        goto fail;
        /* XXX early failure */
    }
    l->conn_limit = UINT32_MAX;
    l->connectionReceived = connectionReceived;
    l->establishmentError = establishmentError;
    l->closed = closed;
    l->connectionError = connectionError;
    return l;
fail:
    if (l) {
        if (l->handles.proto) dlclose(l->handles.proto);
        if (l->base && l->baseCreatedHere) event_base_free(l->base);
        free(l);
    }
    return NULL;
}

void
tapsListenerStop(TAPS_CTX *listener, tapsCallback stopped)
{
    tapsListener     *l = (tapsListener *)listener;

    TAPS_TRACE();
    l->stopped = stopped;
    (l->handles.stop)(l->proto_ctx, &_taps_stopped);
    return;
}

void
tapsListenerDeref(TAPS_CTX *listener)
{
    tapsListener *l = (tapsListener *)listener;
    tapsCallback  stopped = l->stopped;

    l->ref_count--;
    if (l->readyToStop && (l->ref_count == 0)) {
        l->stopped = NULL;
        (*stopped)((TAPS_CTX *)listener, NULL, 0);
    }
}

int
tapsListenerFree(TAPS_CTX *listener)
{
    tapsListener     *l = (tapsListener *)listener;

    TAPS_TRACE();
    if (l->stopped) {
        printf("Trying to free before stopping\n");
        return -1;
    }
    dlclose(l->handles.proto); /* XXX check for errors */
    free(l);
    return 0;
}
