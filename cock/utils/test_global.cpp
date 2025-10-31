#include "cock/utils/logger.hpp"
#include <stdio.h>

namespace cock::test {

using utils::Logger;

class TestGlobal {
public:
    TestGlobal() {
        Logger::trace("TestGlobal initialized!");
    }
};

static TestGlobal tg;

}

