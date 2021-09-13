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


/* These are structs useful for protocol implementations trying to interface
   with taps. */

#include <event2/event.h>
#include <sys/socket.h>
#include <sys/uio.h>

/* Callbacks, from the protocols to TAPS, for various events */
/* The first void is the TAPS context for the component that throws the event.
   For example, a ConnectionReceived event is thrown by the listener. */
/* The second void * is the new, opaque protocol connection context for the
   protocol. It returns the taps context */
typedef void *(*ConnectionReceivedCb)(void *, void *);
typedef void (*EstablishmentErrorCb)(void *); /* XXX add reason code or errno */
typedef void (*StoppedCb)(void *);
/* Connection Context */
typedef void (*SentCb)(void *);
typedef void (*ExpiredCb)(void *);
typedef void (*SendErrorCb)(void *);
typedef void (*ReceivedCb)(void *, void *, size_t);
typedef void (*ReceivedPartialCb)(void *, struct iovec *, size_t);
typedef void (*ReceiveErrorCb)(void *); /* XXX add reason? */
typedef void (*ClosedCb)(void *); /* Connection Closed */
typedef void (*ConnectionErrorCb)(void *);

/* There must be a function "Listen" with the following arguments:
   * void *: an opaque pointer the protocol must return?$
   * struct event_base *: an eventing framework so that the protocol doesn't$
     have to spawn new threads. It can ignore this, and do its own thing, just$
     using the callbacks, but this is likely to be inefficient$
   * struct sockaddr *: the address to bind to.$
   * ConnectionReceivedCb: a function the protocol should call when there's a$
     new connection$
   * EstablishmentErrorCb: callback for listener creation errors$
   * ClosedCb: Callback for a _connection_ spawned by the listener that closes$
   * ConnectionError: Callback for an error on a conn spawned by the listener.

   Returns a pointer to a protocol-specific context, NULL if there's an early
   failure.
*/
/* XXX Add more arguments (SNI, Certificate, ALPN, etc) */
/* Add socket options? */
typedef void *(*listenHandle)(void *, struct event_base *, struct sockaddr *,
        ConnectionReceivedCb, EstablishmentErrorCb, ClosedCb,
        ConnectionErrorCb);
/* Must be a function "Stop" */
typedef void (*stopHandle)(void *, StoppedCb);
/* Must be named "Send" */
/* args: proto context, taps context, data, iovcnt; then callbacks */
typedef void (*sendHandle)(void *, void *, struct iovec *, int, SentCb,
        ExpiredCb, SendErrorCb);
/* Must be named "Receive" */;
/* args: proto context, callbacks */
typedef void (*receiveHandle)(void *, void *, struct iovec *, int,
        ReceivedCb, ReceivedPartialCb, ReceiveErrorCb);
