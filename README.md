# taps
An implementation of TAPS in C with protocol discovery.

The Transport Services API (TAPS) Working Group is standardizing an API that
transport-layer protocols (broadly defined to include substrates above Layer 4,
such as TLS, HTTP, and Websockets) present to applications. Applications
request a transport with certain properties, and TAPS would select the protocol
that best met those requirements. This would free applications from having to
change when new protocols emerge. As the interface is asynchronous, it also
simplifies adapting protocols to various hardware offloads. One objective of
this project is a Linux C implementation of the latest version of the standard.

However, as of 2020 all known TAPS implementations could only select protocols
native to the operating system. Another aim of this project is to implement
"Transport Discovery" -- a means for TAPS to identify the protocol
implementations available provided they meet user-specified trust criteria.

## Getting Started
There are a number of packages you need for this: gcc, make, libyaml-dev,
libevent, libevent-dev.

```sh
$ git clone https://github.com/f5networks/dynamic-taps.git
$ cd dynamic-taps
$ make
$ sudo make install
$ make examples
$ ./echoapp
```
This code launches a server on localhost on port 5555. You can telnet to it,
and the server will echo back whatever you type.

You can also do 'make test' to run the unit tests.

Note: if your dynamic libraries are not in /usr/lib, you will have to modify
kernel.yaml and the Makefile accordingly.

## Architecture

See documents in the docs/ folder.

## Contribution Guidelines

Contributions are welcome. However, F5 corporate policy requires contributors
to complete the Contributor License Agreement in this directory and email it to
j.moses@f5.com.

## References
The interface between applications and TAPS is specified in documents available
on the [TAPS WG github](https://github.com/ietf-tapswg/api-drafts).

The interface between TAPS and arbitrary protocol implementations is defined in
an [individual draft](https://github.com/martinduke/draft-duke-taps-transport-discovery).
