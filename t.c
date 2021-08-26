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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "t.h"

int main(int argc, char *argv[])
{
    int arg, i;
    bool run;

    printf("Assembling test manifest...\n");
    for (i = 0; i < NUM_TESTS; i++) {
        run = (argc == 1);
        for (arg = 1; arg < argc; arg++) {
            if (strcmp(argv[arg], testList[i].name) == 0) {
                run = true;
            }
        }
        if (run) testList[i].test();
    }
}
