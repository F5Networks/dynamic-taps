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
#include <string.h>
#include "t.h"

int
yamlTest()
{
#define NUM_PROTOS_EXPECTED 2
    int numProto, i, j;
    tapsProtocol proto[255];
    int result = 0;

    numProto = tapsUpdateProtocols(proto, 255);
    printf("Number of protocols read: %d\n", numProto);
    if (numProto < NUM_PROTOS_EXPECTED) {
        goto fail;
    }
    for (i = 0; i < numProto; i++) {
        if ((strcmp(proto[i].name, "_kernel_TCP") != 0) &&
                (strcmp(proto[i].name, "_kernel_UDP") != 0)) {
            continue;
        }
        printf("Entry %s Protocol: %s library: %s --\n    ", proto[i].name,
                proto[i].protocol, proto[i].libpath);
        for (j = 0; j < TAPS_NUM_ABILITIES; j++) {
            if ((proto[i].properties.bitmask) & (0x1 << (15-j))) {
                printf("%s, ", tapsPropertyNames[j]);
            }
        }
        printf("\n");
    }
    result = 1;
fail:
    TEST_OUTPUT(result);
    return result;
}
