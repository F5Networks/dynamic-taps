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

#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "taps.h"
#include "taps_debug.h"
#include "taps_protocol.h"

#ifndef _TAPS_INTERNALS_H
#define _TAPS_INTERNALS_H

#define TRUE 1
#define FALSE 0

/* Linked List template. The type must be a struct with an element LIST_ENTRY */
#define LIST_HEAD(name, type)       \
struct name {                       \
    type *head;                     \
    type *tail;                     \
}

#define LIST_INIT(name)             \
    (name)->head = NULL;            \
    (name)->tail = NULL;            

#define LIST_ENTRY(type) type *next;

#define LIST_NEXT(elem)  (elem)->next

#define LIST_EMPTY(list) ((list)->head == NULL)

#define LIST_START(list) ((list)->head)

#define LIST_APPEND(list, elem)      \
    if ((list)->head == NULL) (list)->head = (elem); \
    else (list)->tail->next = (elem);                \
    (elem)->next = NULL;                             \
    (list)->tail = (elem);

#define LIST_PREPEND(list, elem)      \
    if ((list)->tail == NULL) (list)->tail = (elem); \
    (elem)->next = (list)->head      \
    (list)->head = (elem);

/* var = must be declared of the correct type;
 * test = expression that means elem goes *before* element var */
#define LIST_INSERT_ORDER(list, var, elem, test)   \
{                                        \
    (var) = (list)->head;            \
    while ((var) && !(test)) {       \
        (var) = (var)->next;         \
    }                                \
    if ((var) == (list)->head) {     \
        (list)->head = (elem);       \
    }                                \
    if (!(var)) {                    \
        (list)->tail->next = (elem); \
        (list)->tail = (elem);       \
    }                                \
    (elem)->next = (var);            \
}

#define LIST_FOREACH(var, list) \
    for ((var) = (list)->head; (var); (var) = (var)->next)

#define LIST_DELETE_FIRST(list, var) \
{                                                    \
    (var) = (list)->head;                            \
    (list)->head = (var)->next;                      \
    if ((list)->tail == (var)) (list)->tail = NULL;  \
}

/* instructions == additional operations on each element 'var' before free. */
#define LIST_DELETE(list, var, instructions) \
    while (!LIST_EMPTY(list)) {                    \
        LIST_DELETE_FIRST(list, var);              \
        (instructions);                            \
        free(var);                                 \
    }

/* End of LIST macros */

typedef struct _if_list {
    struct ifaddrs ifa;;
    LIST_ENTRY(struct _if_list);
} ifList;

#define TAPS_NUM_ABILITIES 16
typedef struct {
    /* least significant bit */
    uint16_t              reliability : 1;
    uint16_t              preserveMsgBoundaries : 1;
    uint16_t              perMsgReliability : 1;
    uint16_t              preserveOrder : 1;
    uint16_t              zeroRttMsg : 1;
    uint16_t              multistreaming : 1;
    uint16_t              FullChecksumSend : 1;
    uint16_t              FullChecksumRecv : 1;
    uint16_t              congestionControl : 1;
    uint16_t              keepAlive : 1;
    uint16_t              useTemporaryLocalAddress : 1;
    uint16_t              multipath : 1;
    uint16_t              advertises_altaddr : 1;
    uint16_t              direction : 1; /* unidirectional only */
    uint16_t              softErrorNotify : 1;
    uint16_t              activeReadBeforeSend : 1;
    /* most significant bit */
} tapsAbilitiesByName;

typedef union {
    uint16_t              bitmask;
    tapsAbilitiesByName   byName;
} transportAbilities;

typedef struct {
    transportAbilities    require;
    transportAbilities    prefer;
    transportAbilities    avoid;
    transportAbilities    prohibit;
    transportAbilities    explicitlySet;
    tapsMultipath         multipath;
    tapsDirection         direction;
    bool                  advertises_altaddr;
    /* This contains the results of a call to getifaddrs(). If NULL,
       all interfaces are TAPS_IGNORE. If not NULL, each AF_INET and AF_INET6
       instantiation of an interface may contain a pointer in ifa_data to
       tapsPreference; if NULL, it's TAPS_IGNORE */
    struct ifaddrs       *interfaces;
#if 0 /* Not supported at this time */
    void *                pvdRequire;
    void *                pvdPrefer;
    void *                pvdAvoid;
    void *                pvdProhibit;
#endif
} transportProperties;

typedef struct {
    char                *name;
    char                *protocol;
    char                *libpath;
    transportAbilities   properties;
} tapsProtocol;

#if 0
/* Security parameters (Sec 4.3 of draft-ietf-taps-interface) */
typedef struct {
    /* XXX TODO. Just use void pointers for now. */
    void          *myIdentity;
    void          *myPrivateKey;
    void          *myPublicKey;
    size_t         keyLen;
    void          *supported_group;
    void          *ciphersuite;
    void          *signature_algorithm;
    void          *pre_shared_key;
    void          *psk_identity;
    unsigned int   max_cached_sessions;
    unsigned int   cached_session_lifetime_seconds;
    tapsCallback  *trustCallback;
    tapsCallback  *challengeCallback;
} taps_security_params;

/* Elements of the candidate tree */
typedef struct {
    tapsEndpoint                  remote;
    LIST_ENTRY(derivedCandidate);
} derivedCandidate;

typedef struct {
    char                         *libpath;
    int                           pref_score;
    LIST_HEAD(, derivedCandidate) remotes;
    LIST_ENTRY(protoCandidate);
} protoCandidate;

typedef struct {
    tapsEndpoint                 local;
    tapsPreference               pref;
    LIST_HEAD(, protoCandidate)  protos;
    LIST_ENTRY(pathCandidate);
} pathCandidate;
#endif

#define TAPS_MAX_ENDPOINTS 8
#define TAPS_MAX_PROTOCOL_CANDIDATES 256

int tapsUpdateProtocols(tapsProtocol *next, int slotsRemaining);

/* Convenience data structure for TAPS to store the protocol symbols. */
struct proto_handles {
    void             *proto;
    listenHandle      listen;
    stopHandle        stop;
    connectHandle     connect;
    sendHandle        send;
    receiveHandle     receive;
};

/* Called from the preconnection */
TAPS_CTX *tapsListenerNew(void *app_ctx, char *libpath, struct sockaddr *addr,
        struct event_base *base, tapsCallbacks *callbacks);
/* Connections call this; we can't send the Stopped event until all connections
   are dead. */
void tapsListenerDeref(TAPS_CTX *listener);

void _taps_closed(void *taps_ctx);
void _taps_connection_error(void *taps_ctx);
TAPS_CTX *tapsConnectionNew(void *proto_ctx, struct proto_handles *handles,
        TAPS_CTX *listener);
void tapsConnectionInitialize(TAPS_CTX *ctx, void *app_ctx,
        tapsCallbacks *callbacks);
#endif /* _TAPS_INTERNALS_H */
