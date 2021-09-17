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


/* Unit tests for the config tools */

#include <errno.h>
#include <string.h>
#include "t.h"

#define TEST_FN(name) if (name) { goto fail; }

int endpointTest()
{
    int result = 0;
    TAPS_CTX *ep = tapsEndpointNew();
    unsigned char buf[INET6_ADDRSTRLEN];
    struct sockaddr_in sin;

    if (ep == NULL) {
        return result;
    }
    TEST_FN(!tapsEndpointWithHostname(ep, "example.com"));
    TEST_FN(!tapsEndpointWithPort(ep, 443));
    TEST_FN(!tapsEndpointWithService(ep, "h3"));
    TEST_FN(!tapsEndpointWithIPv4Address(ep, "192.168.0.1"))
    TEST_FN(!tapsEndpointWithIPv6Address(ep, "2001:0db8:85a3::8a2e:0370:7334"));
    TEST_FN(!tapsEndpointWithInterface(ep, "eth0"));
    TEST_FN(!tapsEndpointWithProtocol(ep, "UDP"));
    /* Skip alias */
    /* Skip Stun Server */
    /* Try to reload them all */
    TEST_FN(tapsEndpointWithHostname(ep, "example.net"));
    TEST_FN(tapsEndpointWithPort(ep, 80));
    TEST_FN(tapsEndpointWithService(ep, "h2"));
    TEST_FN(tapsEndpointWithIPv4Address(ep, "192.168.0.2"))
    TEST_FN(tapsEndpointWithIPv6Address(ep, "2001:0db8:85a3::8a2e:0370:7335"));
    TEST_FN(tapsEndpointWithInterface(ep, "eth1"));
    TEST_FN(tapsEndpointWithProtocol(ep, "TCP"));

    /* Verify it's all there */
    sin.sin_family = AF_INET;
    if (tapsEndpointGetAddress(ep, (struct sockaddr *)&sin) < 0) goto fail;
    if (sin.sin_port != 443) goto fail;
    if (strcmp(tapsEndpointGetProperty(ep, "ipv4", buf), "192.168.0.1") != 0) {
        goto fail;
    }
    if (strcmp(tapsEndpointGetProperty(ep, "ipv6", buf),
             "2001:db8:85a3::8a2e:370:7334") != 0) {
        goto fail;
    }
    if (strcmp(tapsEndpointGetProperty(ep, "hostname", buf), "example.com") !=
            0) {
        goto fail;
    }
    if (strcmp(tapsEndpointGetProperty(ep, "protocol", buf), "UDP") != 0) {
        goto fail;
    }
    if (strcmp(tapsEndpointGetProperty(ep, "interface", buf), "eth0") != 0) {
        goto fail;
    }
    /* XXX Not able to access this right now */
    //if (endp->has_stun || (endp->stun_credentials != NULL)) goto fail;
    result = 1;
fail:
    TEST_OUTPUT(result);
    if (!result) {
        printf("Error: %s\n", strerror(errno));
    }
    tapsEndpointFree(ep);
    return result;
}
