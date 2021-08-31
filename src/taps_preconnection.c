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

//#include <linux/if.h>
#include <dlfcn.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include "taps_internals.h"

#define MAX_NUM_PROTOCOLS 256
#if 0 /* Use the .yaml for now */
protocols[0] = {
    .protocol_name = "TCP",
    .unique_name   = "_kernel_TCP",
    .libpath       = "taps_tcp.so",
    .properties    = {
        .reliability               = true,
        .preserveMsgBoundaries     = false,
        .perMsgReliability         = false,
        .preserveOrder             = true,
        .zeroRttMsg                = true,
        .multistreaming            = false,
        .FullChecksumSend     = false, /* ? */
        .FullChecksumRecv     = false, /* ? */
        .congestionControl         = true,
        .keepAlive                 = true,
        .useTemporaryLocalAddress  = false,
        .multipath                 = false,
        .advertises_altAddr        = false,
        .direction                 = false,
        .softErrorNotify           = false,
        .activeReadBeforeSend      = true, /* ? */
    },
};

protocols[1] = {
    .protocol_name = "UDP",
    .unique_name   = "_kernel_UDP",
    .libpath       = "taps_udp.so",
    .properties    = {
        .reliability               = false,
        .preserveMsgBoundaries     = true,
        .perMsgReliability         = false,
        .preserveOrder             = false,
        .zeroRttMsg                = true,
        .multistreaming            = false,
        .FullChecksumSend     = true, /* ? */
        .FullChecksumRecv     = true, /* ? */
        .congestionControl         = false,
        .keepAlive                 = false,
        .useTemporaryLocalAddress  = false,
        .multipath                 = false,
        .advertises_altAddr        = false,
        .direction                 = false,
        .softErrorNotify           = false,
        .activeReadBeforeSend      = true, /* ? */
    },
};

#define MARK_PREFERENCE(attr)            \
    switch (transport.attr) {            \
    case TAPS_REQUIRE:                   \
        require.byName.attr = true;      \
        break;                           \
    case TAPS_PREFER:                    \
        prefer.byName.attr = true;       \
        break;                           \
    case TAPS_AVOID:                     \
        avoid.byName.attr = true;        \
        break;                           \
    case TAPS_PROHIBIT:                  \
        prohibit.byName.attr = true;     \
        break;
    }

static void
freeProtocol(tapsProtocol *proto)
{
    free(proto->name);
    proto->name = NULL;
    free(proto->protocol);
    proto->protocol = NULL;
    free(proto->libpath);
    proto->libpath = NULL;
}
#endif

typedef struct {
    tapsEndpoint  *local[TAPS_MAX_ENDPOINTS];
    int            numLocal;
    tapsEndpoint  *remote[TAPS_MAX_ENDPOINTS];
    int            numRemote;
    /* XXX what would be the effect of the application messing with the
       libpath here? Security problem? */
    tapsProtocol   protocol[TAPS_MAX_PROTOCOL_CANDIDATES];
    int            numProtocols;
    transportProperties *transport;
    void                *security;
} tapsPreconnection;

int tapsUpdateProtocols(tapsProtocol *next, int slotsRemaining);

static int
numberOfSetBits(uint32_t i)
{
    i = i - ((i >> 1) & 0x55555555); // eliminate odd bits that match left nbr 
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    i = (i + (i >> 4)) & 0x0f0f0f0f;
    return (i * 0x01010101) >> 24;
}

#define TAPS_MAX_ENDPOINTS     8

TAPS_CTX *
tapsPreconnectionNew(TAPS_CTX **localEndpoint, int numLocal,
        TAPS_CTX **remoteEndpoint, int numRemote,
        TAPS_CTX *transportProps, TAPS_CTX *securityProperties)
{
    tapsPreconnection   *newPreconn = NULL;
    tapsProtocol         proto[255];
    transportProperties *tp = (transportProperties *)transportProps;
    int                  numProtocols, i;

    /* Check for easy problems */
    TAPS_TRACE();
    if ((numLocal > TAPS_MAX_ENDPOINTS) || (numRemote > TAPS_MAX_ENDPOINTS)) {
        errno = E2BIG;
        return NULL;
    }

    newPreconn = malloc(sizeof(tapsPreconnection));
    if (newPreconn == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    memset(newPreconn, 0, sizeof(newPreconn));
    memcpy(newPreconn->local, localEndpoint, sizeof(TAPS_CTX *) * numLocal);
    memcpy(newPreconn->remote, remoteEndpoint, sizeof(TAPS_CTX *) * numRemote);
    newPreconn->numLocal = numLocal;
    newPreconn->numRemote = numRemote;
#if 0
    /* code for a deep copy. Aliases make it very hard */
    epWrite = newPreconn->local;
    for (i = 0; i < numLocal; i++) {
        epRead = ((*localEndpoint) + i);
        memcpy(epWrite, epRead, sizeof(tapsEndpoint));
        epWrite->stun_credentials = NULL;
        epWrite->hostname = strdup(epRead->hostname);
        if (!epWrite->hostname) goto fail;
        epWrite->service = strdup(epRead->service);
        if (!epWrite->service) goto fail;
        epWrite->protocol = strdup(epRead->protocol);
        if (!epWrite->protocol) goto fail;
        /* XXX aliases! these suck */
    }
#endif

    /* Find protocols that work */
    numProtocols = tapsUpdateProtocols(proto, MAX_NUM_PROTOCOLS);
    for (i = 0; i < numProtocols; i++) {
        if ((proto[i].properties.bitmask & tp->prohibit.bitmask) ||
                ((proto[i].properties.bitmask & tp->require.bitmask) !=
                tp->require.bitmask) ||
                (newPreconn->numProtocols == TAPS_MAX_PROTOCOL_CANDIDATES)) {
            /* Fails requirements */
            free(proto[i].name);
            free(proto[i].protocol);
            free(proto[i].libpath);
            continue;
        }
        memcpy(&newPreconn->protocol[newPreconn->numProtocols],
                &proto[i], sizeof(tapsProtocol));
        newPreconn->numProtocols++;
    }
    if (newPreconn->numProtocols == 0) {
        errno = ENOPROTOOPT;
        goto fail;
    }
    newPreconn->transport = tp;
    return newPreconn;
fail:
    tapsPreconnectionFree((TAPS_CTX *)newPreconn);
    return NULL;
}

struct _node {
    void *item;
    int   score;
    LIST_ENTRY(struct _node);
};

int
tapsPreconnectionInitiate(TAPS_CTX *preconn, tapsCallback *ready,
        tapsCallback *error, int timeout)
{
    int                result = 0, i;
    tapsPreconnection *pc = (tapsPreconnection *)preconn;
    tapsConnection    *c, *startc = NULL;
    tapsEndpoint      *ep;
    tapsPreference     pref;
    struct ifaddrs    *ifa;
    LIST_HEAD(, struct _node) paths;
    LIST_HEAD(, struct _node) protos;
    LIST_HEAD(, struct _node) endpoints;
    struct _node      *node, *__node;

    TAPS_TRACE();
    if (pc->numRemote < 1) {
        errno = EINVAL;
        goto fail;
    }

    /* Build a candidate tree */
    /* First, ranked lists of each level */
    for (ifa = pc->transport->interfaces; ifa; ifa = ifa->ifa_next) {
        if ((ifa->ifa_addr->sa_family != AF_INET) &&
                (ifa->ifa_addr->sa_family != AF_INET6)) {
            continue;
        }
        pref = (tapsPreference)(ifa->ifa_data);
        if (pref == TAPS_PROHIBIT) {
            continue;
        }
        node = (struct _node *)malloc(sizeof(struct _node));
        if (!node) goto fail;
        node->item = malloc(strlen(ifa->ifa_name) + 1);
        if (!node->item) {
            free(node);
            goto fail;
        }
        strcpy(node->item, ifa->ifa_name);
        node->score = (int)pref;
        LIST_INSERT_ORDER(&paths, __node, node, node->score > __node->score);
    }
    for (i = 0; pc->numProtocols; i++) {
        if (pc->protocol[i].properties.bitmask &
                pc->transport->prohibit.bitmask) {
            /* Properties have changed; now prohibited */
            continue;
        }
        if (pc->protocol[i].properties.bitmask &
                pc->transport->require.bitmask !=
                pc->transport->require.bitmask) {
            continue;
        }
        node = malloc(sizeof(struct _node));
        if (!node) goto fail;
        node->item = malloc(strlen(pc->protocol[i].libpath) + 1);
        if (!node->item) {
            free(node);
            goto fail;
        }
        strcpy(node->item, pc->protocol[i].libpath);
        node->score = 100 * numberOfSetBits((uint32_t)
                (pc->protocol[i].properties.bitmask &
                 pc->transport->prefer.bitmask)) - numberOfSetBits((uint32_t)
                (pc->protocol[i].properties.bitmask &
                 pc->transport->avoid.bitmask));
        LIST_INSERT_ORDER(&paths, __node, node, node->score > __node->score);
    }
    for (i = 0; i < pc->numRemote; i++) {
        ep = pc->remote[i];
        /* XXX Just list these out by address/port (score = 1);
           leave domain if needed (score = 0). */
    }
    result = 1;
fail:
    while (startc != NULL) {
        c = startc;
        startc = startc->nextCandidate;
        free(c);
    }
    return result;
}

static void
_taps_connection_received(void *taps_ctx, void *proto_ctx)
{
    tapsListener   *listener = taps_ctx;
    tapsConnection *conn = malloc(sizeof(tapsConnection));

    TAPS_TRACE();
    if (conn == NULL) {
        /* XXX Stop the listener */
        return;
    }
    conn->context = proto_ctx;
    conn->state = TAPS_CONNECTED; /* No candidates for servers */
    conn->libpath = listener->protoHandle;
    /* XXX localIf */
    /* XXX remote */
    conn->listener = listener;
    listener->ref_count++;
    conn->nextCandidate = NULL;
    (*(listener->received))(listener, conn, sizeof(tapsConnection));
}

TAPS_CTX *
tapsPreconnectionListen(TAPS_CTX *preconn, struct event_base *base,
        tapsCallback received, tapsCallback error)
{
    int                i;
    tapsPreconnection *pc = (tapsPreconnection *)preconn;
    tapsListener      *l = NULL;
    struct sockaddr_in  sin;
    struct sockaddr_in6 sin6;

    TAPS_TRACE();
    if (pc->numLocal < 1) {
        errno = EINVAL;
        printf("No local endpoints\n");
        goto fail;
    }
    for (i = 0; i < pc->numLocal; i++) {
        if (!pc->local[0]->has_port) {
            errno = EINVAL;
            goto fail;
        }
        if (!pc->local[0]->has_ipv4 && !pc->local[0]->has_ipv6) {
            /* XXX Resolve it, check for IPv6 */
            errno = EINVAL;
            goto fail;
        }
    }
    l = malloc(sizeof(tapsListener));
    if (l == NULL) {
        printf("Couldn't malloc listener\n");
        errno = ENOMEM;
        goto fail;
    }
    /* XXX Pick the best protocol, not just the first */
    l->protoHandle = dlopen(pc->protocol[0].libpath, RTLD_LAZY);
    if (!l->protoHandle) {
        printf("Couldn't get protocol handle: %s\n", dlerror());
        free(l);
        l = NULL;
        goto fail;
    }
    l->listenHandle = dlsym(l->protoHandle, "Listen");
    if (!l->listenHandle) {
        printf("Couldn't get Listen handle: %s\n", dlerror());
        dlclose(l->protoHandle);
        free(l);
        l = NULL;
        goto fail;
    }
    /* We won't need stopHandle for a while, but if it's going to fail, fail it
       now rather than allow listeners to hang */
    l->stopHandle = dlsym(l->protoHandle, "Stop");
    if (!l->stopHandle) {
        printf("Couldn't get ListenStop handle: %s\n", dlerror());
        dlclose(l->protoHandle);
        free(l);
        l = NULL;
        goto fail;
    }
    l->conn_limit = UINT32_MAX;
    l->readyToFree = 0;
    /* XXX Check all the local endpoints */
    /* Just do ipv6 if present, else ipv4, for now */
    if (pc->local[0]->has_ipv6) {
        sin6.sin6_family = AF_INET6;
        sin6.sin6_port = htons(pc->local[0]->port);
        memcpy(&sin6.sin6_addr, &pc->local[0]->ipv6, sizeof(struct in6_addr));
        /* XXX Add error handler */
        l->protoHandle = (l->listenHandle)(l, base, (struct sockaddr *)&sin6,
                &_taps_connection_received, NULL);
    } else {
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = pc->local[0]->ipv4.s_addr;
        sin.sin_port = htons(pc->local[0]->port);
        l->protoHandle = (l->listenHandle)(l, base, (struct sockaddr *)&sin,
                &_taps_connection_received, NULL);
    }
    if (!l->protoHandle) {
        printf("No protoHandle\n");
        goto fail;
        /* XXX early failure */
    }
    l->received = received;
    l->error = error;
    return l;
fail:
    if (l) free(l);
    return NULL;
}

static void
_taps_stopped(void *taps_ctx)
{
    tapsListener *ctx = taps_ctx;

    TAPS_TRACE();
    if (!ctx->stopped) {
        return; /* Can't stop twice! */
    }
    dlclose(ctx->protoHandle); /* XXX check for errors */
    (*(ctx->stopped))((TAPS_CTX *)taps_ctx, NULL, 0);
    ctx->stopped = NULL; /* Mark this as dead */
    ctx->protoHandle = NULL;
}

void
tapsListenerStop(TAPS_CTX *listener, tapsCallback stopped)
{
    tapsListener     *ctx = (tapsListener *)listener;

    TAPS_TRACE();
    ctx->stopped = stopped;
    (ctx->stopHandle)(ctx->protoHandle, &_taps_stopped);
    return;
}

int
tapsListenerFree(TAPS_CTX *listener)
{
    tapsListener     *ctx = (tapsListener *)listener;

    TAPS_TRACE();
    if (!ctx->stopped) {
        return -1;
    }
    ctx->readyToFree = 1;
    if (ctx->ref_count == 0) {
        free(ctx);
    }
    return 0;
}

void
tapsPreconnectionFree(TAPS_CTX *pc)
{
    tapsPreconnection *preconn = (tapsPreconnection *)pc;

    TAPS_TRACE();
    if (preconn) {
        while (preconn->numProtocols) {
            free(preconn->protocol[preconn->numProtocols - 1].name); 
            free(preconn->protocol[preconn->numProtocols - 1].protocol); 
            free(preconn->protocol[preconn->numProtocols - 1].libpath); 
            preconn->numProtocols--;
        }
        free(preconn);
    }
}
