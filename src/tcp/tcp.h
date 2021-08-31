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

#include "../taps_protocol.h"

/* Wrappers for TCP sockets */

/*int initiate(struct sockaddr *remote, char *local, TAPS_CTX *transportProps,
        TAPS_CTX *securityProps);*/
int Listen(void *taps_ctx, struct event_base *base, struct sockaddr *local,
        ConnectionReceivedCb newConnCb, EstablishmentErrorCb error);
void ListenStop(void *proto_ctx, StoppedCb cb);
