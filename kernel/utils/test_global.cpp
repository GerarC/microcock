#include <stdio.h>

class TestGlobal {
public:
    TestGlobal() {
        // printf("[constructor] TestGlobal initialized!\n");
    }
};

static TestGlobal tg;
