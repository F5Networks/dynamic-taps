# TAPS for Protocol Developers

This document explains how to write, modify, or wrap a protocol implementation
so that it can interface with dynamic-taps. The dynamic-taps project is meant
to automatically detect protocols present on a platform.

Further documentation of the concepts this implements is available in the IETF
individual draft [TAPS Protocol Discovery](https://github.com/martinduke/draft-duke-taps-transport-discovery).

This document will refer to the "implementation", which is meant include both
the protocol code itself and any wrapper or add-on that conforms the
implementation to the dynamic-taps requirements.

If you learn best by example, you are welcome to inspect the code that wraps
TCP sockets in the src/tcp/ directory.

## Overview

dynamic-taps assumes that the protocol code is available in a dynamically
linked library (.so file) that contains certain functions that observe certain
conventions about names, return values, and arguments.

It also assumes that the implementation has installed a file in the /etc/taps
directory that describes itself in .yaml. See 'kernel.yaml' for an example of
what this file looks like.

## kernel.yaml

You can list more than one protocol in a file, as in the example. Fields:

* name: is a unique name to identify the implementation.

* protocol: the protocol that is implemented. This if for readability, and is
not used by TAPS.

* libpath: this is where TAPS can find the library.

* properties: this is a list of all the properties the protocol support. Any
omissions are assumed to be FALSE. A full list of valid property names is
defined in src/taps.h as 'char *tapsPropertyNames[]'.

The idea is that the implementation's installer will copy this .yaml file into
the /etc/taps directory. This require admin privileges.

Ultimately, this process will require further authentication and security. The
.yaml file itself will include additional authentication and integrity
checking. TAPS itself will ask for explicit user consent before allowing use
of the library.

## Interfaces

The protocol API is described in src/taps_protocol.h, which is the only TAPS-
specific header file one needs to include.

There are callbacks defined for every TAPS event, which the protocol will use
to notify TAPS of each event. To use these properly, it is important to
understand contexts. There are two relevant levels of context here. TAPS will
provide an opaque pointer for its references to listeners, connections, and
messages, which the protocol MUST include in the arguments for its callbacks.

Also, the protocol will provide TAPS with its own opaque pointer (in the case
of TCP and UDP sockets, just a file descriptor) which TAPS will use when
making calls.

There are also a series of functions that MUST exactly match the provided names
and conform to the function definitions provided.

## Sending and receiving

TAPS will only send one send and receive request (i.e., one of each) at a time
per connection or stream. It buffers the application's other send/receive
commands until it receives a callback providing the disposition of the
previous send or receive.

To support zero-copy, TAPS sends iovec instead of pure buffers. (TODO: receive
iovec as well).

## Eventing and threads

For performance and portability reasons, TAPS uses the libevent framework. An
excellent primer on this library is available here:
[http://www.wangafu.net/~nickm/libevent-book/].

TAPS will provide the implementation with a 'struct event_base' to which it can
add events and therefore avoid creating its own threads. This will obviously
optimize performance, but if necessary the implementation can completely
ignore this and use its own asynchronous framework, as long as it calls the
TAPS-provided callbacks when the corresponding events occur. 
