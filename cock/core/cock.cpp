#include "cock/utils/logger.hpp"
#include <cock/core/cock.hpp>
#include <cock/core/version.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace cock {

using utils::Logger;
using utils::LogLevel;

extern "C" void cock_main(void) {
	Logger::init(LogLevel::TRACE);
	printf("Welcome to %s\n", core::VERSION_STRING);
	printf("Build: %s (%s)\n", COCK_BUILD_DATE, COCK_GIT_HASH);
	puts("Semillero de Linux UdeA");
	puts("SEIC UdeA");

	FOR_ETERNAL;
}

} // namespace cock
