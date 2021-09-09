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

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <yaml.h> // Must install libyaml-dev
#include "taps_internals.h" // includes taps.h

#define TAPS_CONF_PATH "/etc/taps"

typedef enum { NONE, STREAM, DOCUMENT, MAPPING, SEQUENCE }
    yaml_level;

#define STATE_MUST_BE(state) if (eventLevel != (state)) goto fail

static int
tapsParseYaml(FILE *yaml, tapsProtocol *protocol, int space)
{
    /* Borrowed from
       https://github.com/meffie/libyaml-examples/blob/master/parse.c */
    yaml_parser_t parser;
    yaml_event_t event;
    bool stop = false;
    yaml_level eventLevel = NONE;
    char *key = NULL;
    char *field = NULL;
    bool got_name, got_path, got_proto;
    tapsProtocol *proto = protocol;
    tapsProtocol *toDelete;
    int numProtos = 0, i;

    if (!yaml_parser_initialize(&parser)) {
        printf("Parser initialize failed\n");
        return 0;
    }
    yaml_parser_set_input_file(&parser, yaml);

    while (!stop) {
        if (!yaml_parser_parse(&parser, &event)) {
            printf("parser failed\n");
            goto fail;
        }
        /* Expect stream, document, mapping (scalars), mapping, sequence */
        switch(event.type) {
        case YAML_STREAM_START_EVENT:
            STATE_MUST_BE(NONE);
            eventLevel = STREAM;
            break;
        case YAML_STREAM_END_EVENT:
            STATE_MUST_BE(STREAM);
            STATE_MUST_BE(STREAM);
            eventLevel = NONE;
            stop = true;
            break;
        case YAML_DOCUMENT_START_EVENT:
            STATE_MUST_BE(STREAM);
            eventLevel = DOCUMENT;
            memset(proto, 0, sizeof(tapsProtocol));
            got_name = false;
            got_path = false;
            got_proto = false;
            break;
        case YAML_DOCUMENT_END_EVENT:
            STATE_MUST_BE(DOCUMENT);
            if (!got_name || !got_path || !got_proto) {
                goto fail;
            }
            eventLevel = STREAM;
            proto++;
            numProtos++;
            if (numProtos == space) {
                goto out;
            }
            break;
        case YAML_MAPPING_START_EVENT:
            STATE_MUST_BE(DOCUMENT);
            eventLevel = MAPPING;
            break;
        case YAML_MAPPING_END_EVENT:
            STATE_MUST_BE(MAPPING);
            eventLevel = DOCUMENT;
            break;
        case YAML_SEQUENCE_START_EVENT:
            STATE_MUST_BE(MAPPING);
            eventLevel = SEQUENCE;
            break; 
        case YAML_SEQUENCE_END_EVENT:
            STATE_MUST_BE(SEQUENCE);
            eventLevel = MAPPING;
            if (key != NULL) {
                free(key);
                key = NULL;
            }
            break;
        case YAML_SCALAR_EVENT:
            field = strdup((char *)event.data.scalar.value);
            if (key == NULL) {
                key = field;
                field = NULL;
                break;
            }
            if (strcmp(key, "properties") == 0) {
                for (i = 0; i < TAPS_NUM_ABILITIES; i++) {
                    if (strcmp(field, tapsPropertyNames[i]) == 0) {
                        proto->properties.bitmask |= (0x1 << i);
                        break;
                    }
                }
                free(field);
                field = NULL;
                break;
            }
            if (strcmp(key, "name") == 0) {
                proto->name = field;
                got_name = true;
                field = NULL;
                goto reset;
            }
            if (strcmp(key, "protocol") == 0) {
                proto->protocol = field;
                got_proto = true;
                field = NULL;
                goto reset;
            }
            if (strcmp(key, "libpath") == 0) {
                proto->libpath = field;
                got_path = true;
                field = NULL;
                goto reset;
            }
reset:
            free(key);
            key = NULL;
            break;
        default:
            continue;
        }
    }
out:
    yaml_parser_delete(&parser);
    return numProtos;
fail:
    if (key != NULL) {
        free(key);
    }
    if (field != NULL) {
        free(field);
    }
    for (toDelete = protocol; toDelete <= proto; proto++) {
        if (toDelete->protocol != NULL) {
            free(toDelete->protocol);
        }
        if (toDelete->libpath != NULL) {
            free(toDelete->libpath);
        }
    }
    yaml_parser_delete(&parser);
    return 0;
}

int
tapsUpdateProtocols(tapsProtocol *list, int slotsRemaining)
{
    FILE *fptr;
    DIR  *directory;
    char fullPath[255];
    struct dirent *entry;
    int numProtos = 0;
    tapsProtocol *proto = list;

    /* Parse files. */
    directory = opendir(TAPS_CONF_PATH);
    if (directory == NULL) {
        /* Directory is gone, there will be no other protocols */
        return -1;
    }
    sprintf(fullPath, "%s/", TAPS_CONF_PATH);
    for (entry = readdir(directory); entry != NULL;
            entry = readdir(directory)) {
        if (strlen(entry->d_name) < 6) {
            continue;
        }
        if (strcmp(entry->d_name + strlen(entry->d_name) - 4, "yaml") != 0) {
            continue;
        }
        snprintf(fullPath + strlen(TAPS_CONF_PATH) + 1,
                strlen(entry->d_name) + 1, "%s", entry->d_name);
        fptr = fopen(fullPath, "r");
        if (fptr == NULL) {
            continue;
        }
        numProtos = tapsParseYaml(fptr, proto, slotsRemaining - numProtos);
        fclose(fptr);
        proto = list + numProtos;
        if (numProtos == slotsRemaining) {
            break;
        }
    }
    closedir(directory);
    return numProtos;
}
