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
 * Implements the Endpoint object (Sec 4.1 of taps-api)
 */

#include <errno.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include "taps_internals.h"

/* Define an endpoint (Sec 4.1) */
typedef struct _taps_endpoint {
    unsigned char           has_port : 1;
    unsigned char           has_ipv4 : 1;
    unsigned char           has_ipv6 : 1;
    unsigned char           has_hostname : 1;
    unsigned char           has_service : 1;
    unsigned char           has_protocol : 1;
    unsigned char           has_interface : 1;
    unsigned char           has_stun : 1;
    struct in_addr          ipv4;
    struct in6_addr         ipv6;
    uint16_t                port;
    struct sockaddr_in6     stun; /* in6 is bigger than in4; could be either */
    void                   *stun_credentials;
    char                   *hostname;
    char                   *service;
    char                   *protocol;
    char                   *interface;
    struct _taps_endpoint  *prevAlias;
    struct _taps_endpoint  *nextAlias;
} tapsEndpoint;

TAPS_CTX *
tapsEndpointNew()
{
    tapsEndpoint *endp = malloc(sizeof(tapsEndpoint));
    if (endp) memset(endp, 0, sizeof(tapsEndpoint));
    return endp;
}

int
tapsEndpointWithHostname(TAPS_CTX *endp, char *hostname)
{
    tapsEndpoint *ep = endp;
    if (ep->has_hostname) {
        errno = EBUSY;
        return 0;
    }
    ep->hostname = strdup(hostname);
    if (!ep->hostname) {
        errno = ENOMEM;
        return 0;
    }
    ep->has_hostname = true;
    return 1;
}

int
tapsEndpointWithPort(TAPS_CTX *endp, uint16_t port)
{
    tapsEndpoint *ep = endp;
    if (ep->has_port) {
        errno = EBUSY;
        return 0;
    }
    ep->has_port = true;
    ep->port = port;
    return 1;
}

int
tapsEndpointWithService(TAPS_CTX *endp, char *service)
{
    tapsEndpoint *ep = endp;
    if (ep->has_service) {
        errno = EBUSY;
        return 0;
    }
    ep->service = strdup(service);
    if (!ep->service) return 0;
    ep->has_service = true;
    return 1;
}

int
tapsEndpointWithIPv4Address(TAPS_CTX *endp, char *ipv4)
{
    tapsEndpoint *ep = endp;
    if (ep->has_ipv4) {
        errno = EBUSY;
        return 0;
    }
    if (!inet_pton(AF_INET, ipv4, &ep->ipv4)) {
        errno = EINVAL;
        return 0;
    }
    ep->has_ipv4 = true;
    return 1;
}

int
tapsEndpointWithIPv6Address(TAPS_CTX *endp, char *ipv6)
{
    tapsEndpoint *ep = endp;
    if (ep->has_ipv6) {
        errno = EBUSY;
        return 0;
    }
    if (!inet_pton(AF_INET6, ipv6, &ep->ipv6)) {
        errno = EINVAL;
        return 0;
    }
    ep->has_ipv6 = true;
    return 1;
}

int
tapsEndpointWithInterface(TAPS_CTX *endp, char *ifname)
{
    tapsEndpoint *ep = endp;
    if (ep->has_interface) {
        errno = EBUSY;
        return 0;
    }
    ep->interface = strdup(ifname);
    if (!ep->interface) {
        errno = ENOMEM;
        return 0;
    }
    ep->has_interface = true;
    return 1;
}

int
tapsEndpointWithProtocol(TAPS_CTX *endp, char *protoName)
{
    tapsEndpoint *ep = endp;
    if (ep->has_protocol) {
        errno = EBUSY;
        return 0;
    }
    ep->protocol = strdup(protoName);
    if (!ep->protocol) {
        errno = ENOMEM;
        return 0;
    }
    ep->has_protocol = true;
    return 1;
}

int
tapsAddAlias(TAPS_CTX *endp1, TAPS_CTX *endp2)
{
    tapsEndpoint *ep1 = endp1, *ep2 = endp2;

    while (ep1->nextAlias || ep2->prevAlias) {
        if (ep1->nextAlias) ep1 = ep1->nextAlias;
        if (ep2->prevAlias) ep2 = ep2->prevAlias;
    }
    ep1->nextAlias = ep2;
    ep2->prevAlias = ep1;
    return 1;
}

int
tapsWithStunServer(TAPS_CTX *endp, char *addr, uint16_t port,
        void *credentials, size_t credentials_len)
{
    tapsEndpoint *ep = endp;
    unsigned char ipaddr[16];

    if (ep->has_stun) {
        errno = EBUSY;
        return 0;
    }
    if (!inet_pton(AF_INET6, addr, ipaddr)) {
        if (!inet_pton(AF_INET, addr, ipaddr)) {
            errno = EINVAL;
            return 0;
        } else { /* IPv4 */
            struct sockaddr_in *stun4 = (struct sockaddr_in *)&ep->stun;
            stun4->sin_family = AF_INET;
            stun4->sin_port = port;
            memcpy(&stun4->sin_addr, ipaddr, sizeof(stun4->sin_addr));
        }
    } else { /* IPv6 */
        ep->stun.sin6_family = AF_INET6;
        ep->stun.sin6_port = port;
        memcpy(&ep->stun.sin6_addr, ipaddr, sizeof(ep->stun.sin6_addr));
    }
    if (credentials_len > 0) {
        ep->stun_credentials = malloc(credentials_len);
        if (ep->stun_credentials == NULL) {
            errno = ENOMEM;
            return 0;
        }
        memcpy(ep->stun_credentials, credentials, credentials_len);
    }
    return 1;
}

const char *
tapsEndpointGetProperty(TAPS_CTX *endp, char *name, char *buf)
{
    tapsEndpoint *ep = endp;

    if (strcmp(name, "ipv4") == 0) {
        return (ep->has_ipv4 ? inet_ntop(AF_INET, &ep->ipv4, buf, 128) : NULL);
    } 
    if (strcmp(name, "ipv6") == 0) {
        return (ep->has_ipv6 ? inet_ntop(AF_INET6, &ep->ipv6, buf, 128) :
                NULL);
    }
    if (strcmp(name, "hostname") == 0) {
        if (!ep->has_hostname) return NULL;
        memcpy(buf, ep->hostname, strlen(ep->hostname) + 1);
        return buf;
    }
    if (strcmp(name, "service") == 0) {
        if (!ep->has_service) return NULL;
        memcpy(buf, ep->service, strlen(ep->service) + 1);
        return buf;
    }
    if (strcmp(name, "protocol") == 0) {
        if (!ep->has_protocol) return NULL;
        memcpy(buf, ep->protocol, strlen(ep->protocol) + 1);
        return buf;
    }
    if (strcmp(name, "interface") == 0) {
        if (!ep->has_service) return NULL;
        memcpy(buf, ep->interface, strlen(ep->interface) + 1);
        return buf;
    }
    printf("Property not supported\n");
    return NULL;
}

int
tapsEndpointGetAddress(TAPS_CTX *endp, struct sockaddr *saddr)
{
    tapsEndpoint        *ep = endp;
    struct sockaddr_in  *sin = (struct sockaddr_in *)saddr;
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)saddr;

    sin->sin_port = (ep->has_port ? ep->port : 0);
    switch(sin->sin_family) {
    case AF_INET:
        if (ep->has_ipv4) {
            memcpy(&sin->sin_addr, &ep->ipv4, sizeof(struct in_addr));
        } else {
            return -1;
        }
        break;
    case AF_INET6:
        if (ep->has_ipv6) {
            memcpy(&sin6->sin6_addr, &ep->ipv6, sizeof(struct in6_addr));
        } else {
            return -1;
        }
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    return 0;
}

void
tapsEndpointFree(TAPS_CTX *endp)
{
    tapsEndpoint *ep = endp;
    if (ep->has_hostname) free(ep->hostname);
    if (ep->has_service) free(ep->service);
    if (ep->has_protocol) free(ep->protocol);
    if (ep->has_interface) free(ep->interface);
    if (ep->stun_credentials) free(ep->stun_credentials);
    free(ep);
}
