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

#include <sys/socket.h>
#include "taps.h"

/* Callbacks, from the protocols to TAPS, for various events */
/* The first void is the TAPS context for the component that throws the event.
   For example, a ConnectionReceived event is thrown by the listener. */
/* The second void * is hhe new, opaque protocol connection context for the
   protocol. */
typedef void (*ConnectionReceivedCb)(void *, void *);
typedef void (*EstablishmentErrorCb)(void *); /* XXX add reason code or errno */
typedef void (*StoppedCb)(void *);

/* There must be a function "listen" with the following arguments:
   * tapsCtx: an opaque pointer the protocol must return?
   * local: the address to bind to.
   * cb: a function the protocol should call when there's a new connection

   Returns a pointer to a protocol-specific context, NULL if there's an early
   failure. */
/* XXX Add more arguments (SNI, Certificate, ALPN, etc) */
/* Add socket options? */
typedef void *(*listenHandle)(void *, struct event_base *, struct sockaddr *,
        ConnectionReceivedCb, EstablishmentErrorCb);
/* Must be a function "listenStop" */
typedef void (*stopHandle)(void *, StoppedCb);
