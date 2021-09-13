# TAPS for Application Developers

This document explains how to write an application that uses the TAPS API. It
assumes that you've installed dynamic-taps on your machine. It is not meant to
replace the conceptual spec for the taps API that is embedded in multiple IETF
documents, available at the
[TAPS WG github](https://github.com/ietf-tapswg/api-drafts).

If you learn best by example, there are some example applications in the
examples/ directory.

## Context pointers

As TAPS is asynchronous, dynamic-taps is built around callbacks, and these rely
on contexts. When TAPS creates a new object, it will return an opaque pointer
to that context (much like OpenSSL). The application is expected to store that
pointer for use when it wants to perform an operation on the object. This is
just a (void *) but it's aliased as type (TAPS_CTX *) to make it easier to 
identify.

Some TAPS objects (listeners, connections) throw events, and others (endpoints,
transport properties, preconnections) do not. The ones that throw events will
also accept an opaque pointer from the application that provides a return value
for callbacks related to that object. The application may simply provide NULL
for this, in which case TAPS will return its own opaque pointer to that
object instead. This is a simplifying choice when the application doesn't have
any metadata related to the object, and just needs the handle for further
operations on it.

## taps.h

All you need to include to use taps is <taps.h>. (TODO: The makefile does not
yet install this in the system include directory.) As you can see, support for
a large part of the spec does not exist or is commented out. This file defines
a number of interfaces that can be divided into a few groups:

### Callbacks
The first part of the file defines callbacks. Each callback corresponds to an
event defined in the TAPS API draft. The first argument in any callback is a
context that the application previously provided to reference the object that
is throwing the event. For example, a ConnectionReceived event is thrown by the
listener, so the first argument is the application's reference to the Listener.
The connection reference is a different argument, as described in the file's
comments.

As many TAPS objects need multiple callbacks, these are all aggregated into a
single 'struct tapsCallbacks' that can be sent as an argument when needed.
A function that needs these callbacks copies the relevant function pointers and
ignores the rest. It is therefore perfectly safe to have a single instance of
this struct with all the function pointers and send a pointer to it whenever
need it, or make a small change and send it again.

### Objects without events

Endpoints, Transport Properties, Preconnections, and Messages do not throw
events: they are merely data structures. (TODO: some of the struct internals
are currently exposed in taps.h, but these will eventually be hidden behind
object-oriented APIs.) The workflow is straightforward: call ..New(), set and
get fields using the provided functions, and call ..Free() when done.

Note that Preconnections contain Endpoints and Transport Properties. These are
NOT deep-copied; if you modify an Endpoint after attaching it to the
Preconnection, that will affect the parameters of any connections or listeners
created from that Preconnection after the change. Once an application is done
creating listeners and initiating connections, it can safely free all of these
objects; all the relevant information has already been copied into listeners
and connections.

Although Message objects do not throw events, they are arguments in callbacks.
Like objects with events, the application can provide a reference for its
record of the message, or simply accept the TAPS reference. If sent via a Send
or Receive command, DO NOT free the message until a corresponding send- or
receive-related event for that message has returned.

### Objects with Events

Listeners and Connections throw events, and so they store references to
application callback functions to handle those events. They are not created
by ..New() functions, but instead via a Preconnection or a Listener. However,
they still need to be freed when done.

As there is no New() function, getting the application opaque pointer and
callbacks loaded is not straightforward.

The arguments for tapsPreconnectionListen include the application's context
for that listener, as well as callbacks for ConnectionReceived and
EstablishmentError.

The ConnectionReceived callback is the only callback with a return value,
which is the application's context for the new connection. The third argument
is an additional return value pointing to a tapsCallbacks structure with the
Closed and ConnectionError fields filled in.

## Events, asynchronous behavior

dynamic-taps uses the libevent framework, due to its performance and
portability properties. An excellent primer on this library is available here:
[http://www.wangafu.net/~nickm/libevent-book/].

When best utilized, TAPS spawns no threads or processes, and operates as a
library in the calling application's process space.

The TAPS library itself throws no libevent events; it is reliant on the
independent protocol implementations to do so. The most efficient course is for
the application to initialize a 'struct event_base' using event_base_new(),
pass this to TAPS for use, and then call event_base_free() when all networking
operations are complete and the program is done. Doing so eliminates the need
for TAPS to create any additional threads or processes; if the protocol
implementation also supports libevent, there will be no additional threads or
processes at all.

An alternate method (not yet supported) is for the application to simply pass
a NULL event_base to TAPS. In this case, TAPS will create its own event_base
and a thread to wait for events to come in. This means that each Listener and
initiated connection will have its own thread to wait for events, impairing
overall performance.

There is no guarantee that a protocol implementation will use the event_base
provided to it; it may create its own threads. This implementation decision
will be transparent to the application, although the user may be able to
observe the existence of additional threads. However, TAPS will ensure that
events end up at the appropriate application callback. 
