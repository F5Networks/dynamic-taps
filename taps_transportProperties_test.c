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

int transportPropertiesTest()
{
    int result = 0;
    TAPS_CTX *tc = tapsTransportPropertiesNew(TAPS_LISTENER);
    transportProperties *tp = (transportProperties *)tc;
    struct ifaddrs *ifa, *ifList;
    char *ifName;

    if (tc == NULL) {
        return result;
    }
    if (TAPS_IGNORE != 0) {
        printf("We broke Transport Properties because TAPS_IGNORE is not "
                "zero\n");
        return result;
    }
    /* Check listener defaults */
    if (tp->require.bitmask != 0x01c9) goto fail;
    if (tp->prefer.bitmask != 0x0820) goto fail;
    if (tp->avoid.bitmask != 0x0400) goto fail;
    if (tp->prohibit.bitmask != 0x0000) goto fail;
    if (tp->explicitlySet.bitmask != 0x0000) goto fail;
    if (tp->multipath != TAPS_MP_PASSIVE) goto fail;
    if (tp->direction != TAPS_BIDIR) goto fail;
    if (tp->advertises_altaddr) goto fail;
    if (tp->interfaces) goto fail;

    if (getifaddrs(&ifList) != 0) goto fail;
    for (ifa = ifList; ifa; ifa = ifa->ifa_next) {
        if ((ifa->ifa_addr->sa_family == AF_INET) ||
                (ifa->ifa_addr->sa_family == AF_INET6)) {
            ifName = strdup(ifa->ifa_name);
            if (!ifName) return result;
        }
    }
    freeifaddrs(ifList);

    /* Set everything */
    tapsTransportPropertiesSetMultipath(tc, TAPS_MP_ACTIVE);
    tapsTransportPropertiesSetDirection(tc, TAPS_UNIDIR_SEND);
    tapsTransportPropertiesSetAdvertises_altaddr(tc, true);
    TEST_FN(!tapsTransportPropertiesSetInterface(tc, ifName, TAPS_REQUIRE));
    TEST_FN(!tapsTransportPropertiesSet(tc, "reliability", TAPS_PREFER));
    TEST_FN(!tapsTransportPropertiesSet(tc, "preserveMsgBoundaries",
                TAPS_REQUIRE));
    TEST_FN(!tapsTransportPropertiesSet(tc, "perMsgReliability", TAPS_AVOID));
    TEST_FN(!tapsTransportPropertiesSet(tc, "preserveOrder", TAPS_PREFER));
    TEST_FN(!tapsTransportPropertiesSet(tc, "zeroRttMsg", TAPS_REQUIRE));
    TEST_FN(!tapsTransportPropertiesSet(tc, "multistreaming", TAPS_PROHIBIT));
    TEST_FN(!tapsTransportPropertiesSet(tc, "FullChecksumSend",
                TAPS_IGNORE));
    TEST_FN(!tapsTransportPropertiesSet(tc, "FullChecksumRecv",
                TAPS_IGNORE));
    TEST_FN(!tapsTransportPropertiesSet(tc, "congestionControl",
                TAPS_PROHIBIT));
    TEST_FN(!tapsTransportPropertiesSet(tc, "keepAlive", TAPS_PREFER));
    TEST_FN(!tapsTransportPropertiesSet(tc, "useTemporaryLocalAddress",
                TAPS_REQUIRE));
    TEST_FN(!tapsTransportPropertiesSet(tc, "softErrorNotify", TAPS_AVOID));
    TEST_FN(!tapsTransportPropertiesSet(tc, "activeReadBeforeSend",
                TAPS_AVOID));

    /* Verify it's all there */
    if (tp->explicitlySet.bitmask != 0xffff) goto fail;
    if (tp->require.bitmask != 0x0412) goto fail;
    if (tp->prefer.bitmask != 0x3a09) goto fail;
    if (tp->avoid.bitmask != 0xc004) goto fail;
    if (tp->prohibit.bitmask != 0x0120) goto fail;
    if (tp->multipath != TAPS_MP_ACTIVE) goto fail;
    if (tp->direction != TAPS_UNIDIR_SEND) goto fail;
    if (!tp->advertises_altaddr) goto fail;
    if (!tp->interfaces) goto fail;
    for (ifa = tp->interfaces; ifa; ifa = ifa->ifa_next) {
        if ((ifa->ifa_addr->sa_family != AF_INET) &&
                (ifa->ifa_addr->sa_family != AF_INET6)) {
            continue;
        }
        if (strcmp(ifa->ifa_name, ifName) == 0) {
           if (*(tapsPreference *)(&ifa->ifa_data) != TAPS_REQUIRE) {
                goto fail;
            }
        } else {
           if (*(tapsPreference *)(&ifa->ifa_data) != TAPS_IGNORE) {
                goto fail;
            }
        }
    }

    result = 1;
fail:
    TEST_OUTPUT(result);
    if (!result) {
        printf("Error: %s\n", strerror(errno));
    }
    tapsTransportPropertiesFree(tc);
    return result;
}
