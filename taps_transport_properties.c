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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "taps_internals.h"

/* XXX It will accept the non-preference type properties, like multipath! */

TAPS_CTX *
tapsTransportPropertiesNew(tapsConnectionType type)
{
    transportProperties *tp = malloc(sizeof(transportProperties));
    if (!tp) return tp;
    memset(tp, 0, sizeof(transportProperties));
    /* Defaults */
    tp->require.byName.reliability = true;
    tp->require.byName.preserveOrder = true;
    tp->prefer.byName.multistreaming = true;
    tp->require.byName.FullChecksumSend = true;
    tp->require.byName.FullChecksumRecv = true;
    tp->require.byName.congestionControl = true;
    switch(type) {
    case TAPS_LISTENER:
        tp->avoid.byName.useTemporaryLocalAddress = true;
        tp->multipath = TAPS_MP_PASSIVE;
        tp->prefer.byName.multipath = true;
        break;
    case TAPS_INITIATE:
        tp->prefer.byName.useTemporaryLocalAddress = true;
        tp->multipath = TAPS_MP_DISABLED;
        break;
    case TAPS_RENDEZVOUS:
        tp->avoid.byName.useTemporaryLocalAddress = true;
        tp->multipath = TAPS_MP_DISABLED;
        break;
    }
    tp->direction = TAPS_BIDIR;
    return tp;
}

void
tapsTransportPropertiesSetMultipath(TAPS_CTX *tp, tapsMultipath preference)
{
    transportProperties *p = tp;
    p->multipath = preference;
    p->prefer.byName.multipath = (preference != TAPS_MP_DISABLED);
    p->explicitlySet.byName.multipath = true;
}

void
tapsTransportPropertiesSetDirection(TAPS_CTX *tp, tapsDirection preference)
{
    transportProperties *p = tp;
    p->direction = preference;
    p->prefer.byName.direction = (preference != TAPS_BIDIR);
    p->explicitlySet.byName.direction = true;
}

void
tapsTransportPropertiesSetAdvertises_altaddr(TAPS_CTX *tp, bool preference)
{
    transportProperties *p = tp;
    p->advertises_altaddr = preference;
    p->prefer.byName.advertises_altaddr = preference;
    p->explicitlySet.byName.advertises_altaddr = true;
}

int
tapsTransportPropertiesSetInterface(TAPS_CTX *tp, char *name,
        tapsPreference preference)
{
    int result = 0;
    struct ifaddrs      *ifa;
    transportProperties *p = tp;

    if (p->interfaces == NULL) {
        if (preference == TAPS_IGNORE) {
            return 1; /* don't bother */
        }
        if (getifaddrs(&p->interfaces) != 0) {
            return 0;
        }
    }
    for (ifa = p->interfaces; ifa; ifa = ifa->ifa_next) {
        if (((ifa->ifa_addr->sa_family != AF_INET) &&
                 (ifa->ifa_addr->sa_family != AF_INET6)) ||
                (strcmp(ifa->ifa_name, name) != 0)) {
            continue;
        }
        /* this is gross; writing an enum to a void *! */
        memcpy(&ifa->ifa_data, &preference, sizeof(preference));
    }
    return 1;
}

int
tapsTransportPropertiesSet(TAPS_CTX *tp, char *propertyName,
        tapsPreference preference)
{
    transportAbilities  *vector, *off1, *off2, *off3;
    transportProperties *p = tp;
    int i;
    uint16_t mask;

    for (i = 0; i < TAPS_NUM_ABILITIES; i++) {
        if (strcmp(propertyName, tapsPropertyNames[i]) == 0) {
            break;
        }
    }
    if (i == TAPS_NUM_ABILITIES) {
        errno = EINVAL;
        return 0;
    }
    mask = (0x1 << i);
    /* Clear bit everywhere */
    p->require.bitmask &= (~mask);
    p->prefer.bitmask &= (~mask);
    p->avoid.bitmask &= (~mask);
    p->prohibit.bitmask &= (~mask);
    p->explicitlySet.bitmask |= mask;
    switch(preference) {
    case TAPS_REQUIRE:
        p->require.bitmask |= mask;
        break;
    case TAPS_PREFER:
        p->prefer.bitmask |= mask;
        break;
    case TAPS_AVOID:
        p->avoid.bitmask |= mask;
        break;
    case TAPS_PROHIBIT:
        p->prohibit.bitmask |= mask;
        break;
    }
    return 1;
}

void tapsTransportPropertiesFree(TAPS_CTX *tp)
{
    transportProperties *p = tp;

    if (p->interfaces) {
        freeifaddrs(p->interfaces);
    }
    free(tp);
}
