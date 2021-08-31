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

/*
 * This is the main taps API that applications can leverage.
 *
 * Note: to maximize applicability, and to allow possible use in the
 * kernel, this is written in pure C. We can write a wrapper in C++ to make
 * it easier for applications to use these interfaces.
 */

/*
 * Connection properties.
 *
 * Reference: draft-ietf-taps-interface, section 4.2.
 */

#ifndef _TAPS_H
#define _TAPS_H

#include <arpa/inet.h>
#include <event2/event.h>
#include <event2/util.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define TAPS_DEBUG

#ifdef TAPS_DEBUG
#define TAPS_TRACE() printf("Function %s: %s\n", __FILE__, __FUNCTION__);
#else
#define TAPS_TRACE()
#endif

typedef void TAPS_CTX;

/* Function pointers for callbacks */
typedef void (*tapsCallback)(TAPS_CTX *, void *, size_t);

/* Connection properties (Sec 6) */
typedef enum { TAPS_ESTABLISHING, TAPS_ESTABLISHED, TAPS_CLOSING, TAPS_CLOSED }
    tapsConnectionState;

typedef enum { TAPS_FCFS, TAPS_RR, TAPS_RR_PKT, TAPS_PRIO, TAPS_FC, TAPS_WFQ }
    tapsScheduler;

typedef enum { TAPS_DEFAULT, TAPS_SCAVENGER, TAPS_INTERACTIVE_CAP,
    TAPS_NON_INTERACTIVE, TAPS_CONSTANT_RATE, TAPS_CAPACITY_SEEKING }
    tapsCapacity;

#if 0
typedef enum { TAPS_HANDOVER, TAPS_INTERACTIVE_MP, TAPS_AGGREGATE }
    tapsMultipathPolicy;

/* Returns the final length of out */
typedef size_t (*tapsFramer)(void *in, void *out, size_t in_len);

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

/* Most taps functions require the caller to provide a callback */
typedef void (*tapsHandler)(int fd, int error);

/*
 * Here are the main function definitions. Section references are for
 * draft-ietf-taps-interface.
 */

/* Endpoint functions. See Sec 4.1. */
/* Create a new endpoint instance */
TAPS_CTX *tapsEndpointNew();
/* Attachment functions. All return 1 on success, and 0 on failure with errno
   set. */
int tapsEndpointWithHostname(TAPS_CTX *endp, char *hostname);
int tapsEndpointWithPort(TAPS_CTX *endp, uint16_t port);
int tapsEndpointWithService(TAPS_CTX *endp, char *service);
int tapsEndpointWithIPv4Address(TAPS_CTX *endp, char *ipv4);
int tapsEndpointWithIPv6Address(TAPS_CTX *endp, char *ipv6);
int tapsEndpointWithInterface(TAPS_CTX *endp, char *ifname);
int tapsEndpointWithProtocol(TAPS_CTX *endp, char *protoName);
/* Alias two endpoints together */
int tapsAddAlias(TAPS_CTX *endp1, TAPS_CTX *endp2);
/* Attach a stun server. */
/* XXX No support for doing anything with this */
int tapsWithStunServer(TAPS_CTX *endp, char *addr, uint16_t port,
        void *credentials, size_t credentials_len);
/* Clean up the instance. */
void tapsEndpointFree(TAPS_CTX *endp);

/* Transport property functions. See Sec 4.2. */
typedef enum { TAPS_LISTENER, TAPS_INITIATE, TAPS_RENDEZVOUS }
    tapsConnectionType;
typedef enum  { TAPS_REQUIRE = 2, TAPS_PREFER = 1, TAPS_IGNORE = 0,
    TAPS_AVOID = -1, TAPS_PROHIBIT = -2} tapsPreference;
typedef enum { TAPS_MP_DISABLED, TAPS_MP_ACTIVE, TAPS_MP_PASSIVE }
    tapsMultipath;
typedef enum { TAPS_BIDIR, TAPS_UNIDIR_SEND, TAPS_UNIDIR_RECV } tapsDirection;
/* Sets defaults according to connection type */
TAPS_CTX *tapsTransportPropertiesNew(tapsConnectionType type);
void tapsTransportPropertiesSetMultipath(TAPS_CTX *tp,
        tapsMultipath preference);
void tapsTransportPropertiesSetDirection(TAPS_CTX *tp,
        tapsDirection preference);
void tapsTransportPropertiesSetAdvertises_altaddr(TAPS_CTX *tp,
        bool preference);
/* For Interfaces and Provisioning Domains */
/* XXX No current support for PvDs */
int tapsTransportPropertiesSetInterface(TAPS_CTX *tp, char *name,
        tapsPreference preference);
/* All other properties that use tapsPreference */
/* Valid property names:
   "reliability", "preserveMsgBoundaries", "perMsgReliability",
    "preserveOrder", "zeroRttMsg", "multistreaming",
    "FullChecksumSend", "FullChecksumRecv", "congestionControl",
    "keepAlive", "useTemporaryLocalAddress", "softErrorNotify",
    "activeReadBeforeSend",
 */
static char *tapsPropertyNames[] = {
    "reliability", "preserveMsgBoundaries", "perMsgReliability",
    "preserveOrder", "zeroRttMsg", "multistreaming",
    "FullChecksumSend", "FullChecksumRecv", "congestionControl",
    "keepAlive", "useTemporaryLocalAddress", "multipath",
    "advertises_altaddr", "direction", "softErrorNotify",
    "activeReadBeforeSend",
};
int tapsTransportPropertiesSet(TAPS_CTX *tp, char *propertyName,
        tapsPreference preference);
void tapsTransportPropertiesFree(TAPS_CTX *tp);

/* "local" and "remote" point to an array of endpoints */
/*
 * Warning! Calling tapsPreconnectionNew does not freeze the contents of the
 * endpoint and property objects. This data is not frozen until the connection
 * begins. example, if I do the following:
 * local = tapsEndpointNew();
 * tp = tapsTransportPropertiesNew(TAPS_LISTENER);
 * pc = tapsPreconnectionNew(local, 1, NULL, 0, tp, NULL);
 * tapsEndpointWithPort(local, 80);
 * listener = tapsPreconnectionListen(pc);
 *
 * This will cause TAPS to listen on port 80 on at least one protocol that
 * matches the properties. After assigning listener, it is safe to change the
 * port of local without affecting the port of the first listening socket.
 *
 * Freeing the properties or endpoint, and then trying to connect using that
 * preconnection, is likely to segfault your code.
 */
TAPS_CTX *tapsPreconnectionNew(TAPS_CTX **localEndpoint, int numLocal,
        TAPS_CTX **remoteEndpoint, int numRemote,
        TAPS_CTX *transportProps, TAPS_CTX *securityParameters);
/* Initiate a connection. This will ignore any Local Endpoints in the preconn.
   Arguments:
   ready: callback function when ready for use. the "data" argument will be
          a pointer to the connection context (of type TAPS_CTX).
   error: callback if somehting goes wrong.
   timeout: time (in milliseconds) to wait before giving up.
   Returns: a pointer to the listener object
 */
/* Focus on the server case, for now.
int tapsPreconnectionInitiate(TAPS_CTX *preconn, tapsCallback *ready,
        tapsCallback *error, tapsCallback *close, tapsCallback *abort,
        int timeout);
*/

#if 0
/* Sec 5.1  Returns an fd for the connection */
int tapsInitiateWithSend(int preconnection, unsigned int timeoutSec,
        void *data, size_t data_len, tapsMessageContext *ctx,
        tapsCallback *ready, tapsCallback *error, tapsCallback *sent);
#endif

/* Sec 5.2 Returns an fd for the listener. The "received" callback will
 * reference  this fd and the "data" field will be a ptr to int with the fd of
 * the connection. */
/* We need to include handlers for connection close and abort that will be
   installed in connections that are returned with received(). */
TAPS_CTX *tapsPreconnectionListen(TAPS_CTX *preconn, struct event_base *base,
        tapsCallback received, tapsCallback error);
void tapsPreconnectionFree(TAPS_CTX *pc);
void tapsListenerStop(TAPS_CTX *listener, tapsCallback stopped);
int tapsListenerFree(TAPS_CTX *listener);

#if 0
/* If limit == 0, it is infinite */
void tapsListenerNewConnectionLimit(int listener, unsigned int limit);

/* Sec 5.3 Returns a file descriptor for the rendezvous */
int tapsRendezvous(int preconnection, tapsCallback *rendezvousDone,
        tapsCallback *error);
void tapsResolve(int preconnection, tapsEndpoint **local,
        tapsEndpoint **remote);
void tapsAddRemote(int preconnection, tapsEndpoint *remote);

/* Sec 5.4. Returns an fd for the clone */
int tapsClone(int connection, int *group, tapsCallback *connected,
        tapsCallback *error);

/* Sec 6. Connection Properties. "value" a void pointer cast to the
   correct type. */
/* See the typedefs at the end of this file to see the legitimate property
   names and typecast for value. */
void tapsSetProperty(int connection, char *propertyName, void *value);
void tapsGetProperty(int connection, char *propertyName, void *value);
/* It would be good to allow the application to attach metadata, I think. */

/* XXX TODO Messages and Framers (Sec 7.1) */
#endif

/* Sending (Sec 7.2) */
void tapsConnectionSend(TAPS_CTX *connection, void *data, size_t data_len,
        tapsCallback *sent, tapsCallback *expired, tapsCallback *error);
#if 0
void tapsStartBatch(int connection);
void tapsEndBatch(int connection);
#endif

/* Receiving (Sec 7.3) */
/*
 * Set minIncompleteLength >= maxLength to have no limit; 
 * maxLength == 0: no limit
 */
void tapsConnectionReceive(TAPS_CTX *connection, size_t minIncompleteLength,
        size_t maxLength, tapsCallback *received, tapsCallback *receivedPartial,
        tapsCallback *error);

#if 0
/* Termination (Sec 8) */
void tapsClose(int connection, tapsCallback *closed, tapsCallback *error);
void tapsAbort(int connection, tapsCallback *error);
void tapsCloseGroup(int connection);
void tapsAbortGroup(int connection);

/* We could just free the connection on the closed event, but the application
   might want to query metadata to free its state */
void tapsConnectionFree(TAPS_CTX *connection);
#endif

#if 0
/* Applications should never have to use the structures below, but they are a
   useful reference for proper use of tapsSetProperty and tapsGetProperty. */

typedef struct {
    tapsConnectionState        state;
    char                      *protocol;
    bool                       sendOk;
    bool                       recvOk;
    transportProperties       *transportProperties;
    /* read-write */
    int                        checkSumLen; /* -1 = full coverage */
    int                        connPrio;
    int                        connTimeout; /* in seconds. -1 = disabled */
    int                        keepAliveTimeout; /* in sec. -1 = disabled */
    tapsScheduler              connScheduler;
    tapsCapacity               connCapacityProfile;
    tapsMultipathPolicy        multipath_policy;
    unsigned long              minSendRate, minRecvRate, maxSendRate,
                               maxRecvRate; /* max = 0; unlimited */
    long int                   groupConnLimit; /* -1 = unlimited */
    bool                       isolateSession;
    /* Below this line, read-only */
    unsigned int               zeroRttMsgMaxLen;
    unsigned int               singularTransmissionMsgMaxLen;
    unsigned int               sendMsgMaxLen;
    unsigned int               recvMsgMaxLen;
    /* TCP-only */
    unsigned int               tcpUserTimeoutValue;
    bool                       tcpUserTimeout
    bool                       tcpUserTimeoutRecv;
    /* Callbacks */
    tapsCallback               softError;
    tapsCallback               pathChallenge;
} tapsConnectionProperties;

const tapsConnectionProperties tapsConnectionDefaults = {
    .state                     = TAPS_ESTABLISHING,
    /* Skip protocol */
    .sendOK                    = true,
    .recvOK                    = true,
    .transportProperties       = NULL,
    .checkSumLen               = -1,
    .connPrio                  = 100,
    .connTimeout               = -1,
    /* No universal default for keepAliveTimeout */
    .connScheduler             = TAPS_WFQ,
    .connCapacityProfile       = TAPS_DEFAULT,
    .multipath_policy          = TAPS_HANDOVER,
    .minSendRate               = 0,
    .minRecvRate               = 0,
    .maxSendRate               = 0,
    .maxRecvRate               = 0,
    .groupConnLimit            = -1,
    .isolateSession            = false,
    /* Skip read only fields */
    .tcpUserTimeoutValue       = false,
    .tcpUserTimeoutRecv        = true,
    .softError                 = NULL,
    .pathChange                = NULL,
};
#endif

#endif /* _TAPS_H */
