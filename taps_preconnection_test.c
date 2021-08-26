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

int preconnectionTest()
{
    int result = 0;
    tapsEndpoint *local = (tapsEndpoint *)tapsEndpointNew();
    transportProperties *tp = (transportProperties *)
            tapsTransportPropertiesNew(TAPS_LISTENER);
    tapsPreconnection *pc;
    
    if (!local || !tp) {
        return result;
    }
    TEST_FN(!tapsEndpointWithPort(local, 80));
    pc = (tapsPreconnection *)tapsPreconnectionNew((TAPS_CTX **)&local, 1, NULL,
            0, (TAPS_CTX *)tp, NULL);
    if (!pc) goto fail;
    /* Verify it's all there */
    if (pc->local[0] != local) goto fail;
    if (pc->numLocal != 1) goto fail;
    if (pc->numRemote != 0) goto fail;
    if (pc->numProtocols != 1) goto fail;
    if (strcmp(pc->protocol[0].name, "_kernel_TCP") != 0) goto fail;
    if (strcmp(pc->protocol[0].protocol, "TCP") != 0) goto fail;
    if (strcmp(pc->protocol[0].libpath,
                "/home/mduke/taps/lib/libtaps_tcp.so") != 0) goto fail;
    if (pc->transport != tp) goto fail;
    if (pc->security != NULL) goto fail;
    result = 1;
fail:
    TEST_OUTPUT(result);
    if (!result) {
        printf("Error: %s\n", strerror(errno));
    }
    tapsPreconnectionFree(pc);
    return result;
}
