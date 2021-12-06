# TAPS for Internal Developers

This document is meant to introduce the project to developers interested in TAPS internals.

## Reference Material

It is very helpful to understand the
[TAPS interface specification](https://datatracker.ietf.org/doc/draft-ietf-taps-interface/)
and the
[Transport Discovery Proposal](https://datatracker.ietf.org/doc/draft-duke-taps-transport-discovery/).

## Design principles

* This software strives to be object-oriented, in spite of being written in
plain C. The current design typically uses an opaque handle to refer to
objects with a number of functions.

* Typically, the internals of the structure a context pointer points to will
be defined in the .c file so that it is not easily accessible by other entities.

* Please seek to use libraries that are as portable across the Unix family as
possible. This was an important factor in choosing libevent for the default
event architecture.

* Be aware that both applications and protocol implementations will often be
mostly legacy code. Therefore, please design interfaces to accommodate
alternative design choices (e.g. don't fail if an application or protocol uses
an event framework other than libevent).

* All events between the protocol, TAPS, and the application are handled by
function callbacks, to reduce the dependency on any event framework.

* Production-quality scalability is an important consideration. Try to minimize
copying of application data, etc. For this reason, TAPS accepts iovecs as an
input, rather than requiring data to be written into a continuous buffer.

* Provide sensible defaults that minimize the amount of code that applications
have to write in the most common use cases.

* Use verbatim names from the TAPS interface spec wherever possible;

## Coding Style

These are still under consideration, but here are some initial guidelines that
are usually followed in the existing codebase:

* Indent is 4 spaces, except continuation of the same line is indented 8 spaces.

* Aside from the first word in a variable or function name, the first letter in
each word is capitalized. There are exceptions for names that are taken verbatim
from the interface spec.

* Constants are all-caps.

* Functions are of the format:

~~~
return_type
function_name(arg1_type arg1_name, arg2_type arg2_name, ...)
{
    type local_variable;

    function_line_1;
    function_line_2;
}
~~~

* Functions that are callbacks have names that begin with _ and use spaces
between words, all lower case.

* TAPS interface functions are named with the format
taps<Object_name><InterfaceName>, e.g. tapsPreconnectionListen().

* Object contexts are of type TAPS_CTX, which is just a void.

* Use the string "XXX" in a comment to indicate major missing pieces.

* All code files begin with the copyright and license information visible in
any file in the src/ directory.

* Non-functional code that may be of future use may be wrapped in either an
'#if 0' compiler directive, or #ifdef with some flag if you need a flag to turn
it on.

* Rather than pass all event callback handles individually, any API function
that needs them simply uses the struct tapsCallbacks, which has space for
pointers to the handlers for every TAPS event. It should check that the
required pointers for that API function are actually set.

## Code structure

* Each object defined in the TAPS interface spec has a corresponding file in
the src/ directory. The other C file in this directory is taps_cfg.c, which
reads the YAML files and loads the protocol directory into memory. This is
currently called by the preconnection, but ultimately tapsd will use it.

* There are four header files: taps.h is the API to applications.
taps_protocol.h indicates the interface that protocols must support to work
with TAPS. taps_internals.h are common items not available to protocols or
applications, but are avaiable between TAPS objects. Finally, taps_debug.h
has the flag to turn off debug logging.

## Unit Tests

* Although the current code does not live up to this standard, please create
unit tests in the test/ directory for each object in the src/ directory.

* Each object's tests should go in their own file. When adding a test suite,
developers should update the testList struct in t.h and include t.h.

* Each test should conclude with a call to the TEST_OUTPUT macro in t.h.

## Debuggability

* Include 'TAPS_TRACE()' at the start of each function, so that debugging mode
will log every entrance to that function.

* Please set errno and add a printf for any unexpected error conditions (and
gracefully exit from such conditions!)
