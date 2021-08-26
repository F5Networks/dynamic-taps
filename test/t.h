#ifndef _T_H
#define _T_H

#include <stdio.h>
#include "../src/taps_internals.h"

#define TEST_OUTPUT(result) printf("%s: %s\n", __func__, result ? "success" : \
   "failed")

#define NUM_TESTS (sizeof(testList)/sizeof(testList[0]))

struct _test_entry {
    char *name;
    int (*test)(void);
};

extern int yamlTest();
extern int endpointTest();
extern int transportPropertiesTest();
extern int preconnectionTest();

static const struct _test_entry testList[] = {
    { "yaml", yamlTest },
    { "endpoint", endpointTest },
    { "transportProperties", transportPropertiesTest },
    { "preconnection", preconnectionTest},
};

#endif /* _T_H */
