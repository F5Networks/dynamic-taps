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
    TAPS_CTX      *local[TAPS_MAX_ENDPOINTS];
    int            numLocal;
    TAPS_CTX      *remote[TAPS_MAX_ENDPOINTS];
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
    tapsPreconnection   *pc = NULL;
    tapsProtocol         proto[255];
    transportProperties *tp = (transportProperties *)transportProps;
    int                  numProtocols, i;

    /* Check for easy problems */
    TAPS_TRACE();
    if ((numLocal > TAPS_MAX_ENDPOINTS) || (numRemote > TAPS_MAX_ENDPOINTS)) {
        errno = E2BIG;
        return NULL;
    }

    pc = malloc(sizeof(tapsPreconnection));
    if (pc == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    memset(pc, 0, sizeof(pc));
    memcpy(pc->local, localEndpoint, sizeof(TAPS_CTX *) * numLocal);
    memcpy(pc->remote, remoteEndpoint, sizeof(TAPS_CTX *) * numRemote);
    pc->numLocal = numLocal;
    pc->numRemote = numRemote;
#if 0
    /* code for a deep copy. Aliases make it very hard */
    epWrite = pc->local;
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
                (pc->numProtocols == TAPS_MAX_PROTOCOL_CANDIDATES)) {
            /* Fails requirements */
            free(proto[i].name);
            free(proto[i].protocol);
            free(proto[i].libpath);
            continue;
        }
        memcpy(&pc->protocol[pc->numProtocols],
                &proto[i], sizeof(tapsProtocol));
        pc->numProtocols++;
    }
    if (pc->numProtocols == 0) {
        errno = ENOPROTOOPT;
        goto fail;
    }
    pc->transport = tp;
    return pc;
fail:
    tapsPreconnectionFree((TAPS_CTX *)pc);
    return NULL;
}

struct _node {
    void *item;
    int   score;
    LIST_ENTRY(struct _node);
};

struct proto_handles *
tapsFetchHandles(char *libpath, bool listener)
{
    struct proto_handles *handles = malloc(sizeof(struct proto_handles));
    if (!handles) {
        goto fail;
    }
        
    handles->proto = dlopen(libpath, RTLD_LAZY);
    if (!handles->proto) {
        printf("Couldn't get protocol handle: %s\n", dlerror());
        goto fail;
    }
    if (listener) {
        handles->listen = dlsym(handles->proto, "Listen");
        if (!handles->listen) {
            printf("Couldn't get Listen handle: %s\n", dlerror());
            goto fail;
        }
        /* Fail fast if the protocol does not have all the required handles.
           This vetting could occur in tapsd? The preconnection? */
        handles->stop = dlsym(handles->proto, "Stop");
        if (!handles->stop) {
            printf("Couldn't get Stop handle: %s\n", dlerror());
            goto fail;
        }
    } else {
        handles->connect = dlsym(handles->proto, "Connect");
        if (!handles->connect) {
            printf("Couldn't get Connect handle: %s\n", dlerror());
            goto fail;
        }
    }
    handles->send = dlsym(handles->proto, "Send");
    if (!handles->send) {
        printf("Couldn't get Send handle: %s\n", dlerror());
        goto fail;
    }
    handles->receive = dlsym(handles->proto, "Receive");
    if (!handles->receive) {
        printf("Couldn't get Receive handle: %s\n", dlerror());
        goto fail;
    }

    return handles;
fail:
    if (handles) {
        if (handles->proto) dlclose(handles->proto);
        free(handles);
    }
    return NULL;
}

TAPS_CTX *
tapsPreconnectionInitiate(TAPS_CTX *preconn, void *app_ctx,
        struct event_base *base, tapsCallbacks *callbacks, int timeout)
{
    TAPS_CTX             *conn = NULL;
    tapsPreconnection    *pc = (tapsPreconnection *)preconn;
    void                 *proto_ctx;
    struct proto_handles *handles = NULL;
#if 0
    int                i;
    tapsEndpoint      *ep;
    tapsPreference     pref;
    struct ifaddrs    *ifa;
    LIST_HEAD(, struct _node) paths;
    LIST_HEAD(, struct _node) protos;
    LIST_HEAD(, struct _node) endpoints;
    struct _node      *node, *__node;
#endif

    TAPS_TRACE();
    if (pc->numRemote < 1) {
        errno = EINVAL;
        goto fail;
    }

    /* Just take the first protocol for now */
    handles = tapsFetchHandles(pc->protocol[0].libpath, true);
    if (!handles) {
        return NULL;
    }
    conn = tapsConnectionNew(app_ctx, handles, pc->remote[0]);
    if (!conn) {
        printf("couldn't get socket\n");
        goto fail;
    }
    return conn;
fail:
    if (conn) {
        tapsConnectionFree(conn);
    }
    if (handles) {
        free(handles);
    }
    return NULL;
}

#if 0
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
#endif

TAPS_CTX *
tapsPreconnectionListen(TAPS_CTX *preconn, void *app_ctx,
        struct event_base *base, tapsCallbacks *callbacks)
{
    int                 i;
    tapsPreconnection  *pc = (tapsPreconnection *)preconn;
    TAPS_CTX           *l = NULL;
    struct sockaddr_in  sin;
    struct sockaddr_in6 sin6;
    struct sockaddr    *addr;
    struct proto_handles *handles;

    TAPS_TRACE();
    if (!pc || (pc->numLocal < 1) || !callbacks ||
            !callbacks->connectionReceived || !callbacks->establishmentError) {
        errno = EINVAL;
        printf("Missing preconnection arguments\n");
        return NULL;
    }
    if (!base) {
        errno = EINVAL;
        printf("Base = NULL not yet supported\n"); /* XXX */
        return NULL;
    }
    /* XXX Pick the best protocol, not just the first */
    /* XXX Check all the local endpoints */
    sin6.sin6_family = AF_INET6;
    /* Just do ipv6 if present, else ipv4, for now */
    addr = (struct sockaddr *)&sin6;
    if (tapsEndpointGetAddress(pc->local[0], addr) < 0) {
        sin.sin_family = AF_INET;
        addr = (struct sockaddr *)&sin;
        if (tapsEndpointGetAddress(pc->local[0], addr) < 0) {
            errno = EINVAL;
            return NULL;
        } else {
	    sin.sin_port = htons(sin.sin_port);
            if (sin.sin_port == 0) {
                errno = EINVAL;
                return NULL;
            }
        }
    } else {
        sin6.sin6_port = htons(sin6.sin6_port);
        if (sin6.sin6_port == 0) {
            errno = EINVAL;
            return NULL;
        }
    }

    handles = tapsFetchHandles(pc->protocol[0].libpath, true);
    if (!handles) {
        return NULL;
    }
    l = tapsListenerNew(app_ctx, handles, addr, base, callbacks);
    free(handles);
    return l;
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
