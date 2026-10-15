#include "probe.hpp"
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr size_t CHECK_COUNT = 34;
constexpr uintptr_t EXIT_ALL_PASSED = 0;
constexpr uintptr_t EXIT_SOME_FAILED = 1;

} // namespace

extern "C" void _start() {
	bool results[CHECK_COUNT] = {
		probe::portsAreDenied(),
		probe::irqIsDenied(),
		probe::grantIsDenied(),
		probe::physicalVideoMapIsDenied(),
		probe::physicalRamMapIsDenied(),
		probe::kernelWriteIsDenied(),
		probe::unmappedWriteIsDenied(),
		probe::anonymousMemoryWorks(),
		probe::kernelUnmapIsDenied(),
		probe::irqNotificationIsRefused(),
		probe::capabilityGrantIsRefused(),
		probe::messageToSelfArrives(),
		probe::tryReceiveDoesNotBlock(),
		probe::oversizedWriteIsCut(),
		probe::mappingQuotaIsEnforced(),
		probe::releasedAddressIsReused(),
		probe::regionTableIsBounded(),
		probe::printfFormatsAndLongLines(),
		probe::mallocReusesAndCoalesces(),
		probe::reallocKeepsContents(),
		probe::callocZeroesRecycledMemory(),
		probe::hugeAllocationFailsCleanly(),
		probe::operatorNewRoundTrips(),
		probe::sharedMemoryRoundTrips(),
		probe::sharedMemoryRefusesBadRequests(),
		probe::sharedMemoryTableIsBounded(),
		probe::bootModulesAreDenied(),
		probe::initrdFileReads(),
		probe::fileServerRefusesBadRequests(),
		probe::fileServerListsDirectories(),
		probe::manifestMatchesTheBoot(),
		probe::moduleIndexIsDenied(),
		probe::spawnIsDenied(),
		probe::processAliveReportsLiveness()};

	unsigned passed = 0;
	for (size_t idx = 0; idx < CHECK_COUNT; idx++) {
		if (results[idx]) {
			passed++;
			continue;
		}
		printf("probe: check %u FAILED\n", static_cast<unsigned>(idx + 1));
	}

	printf("probe: %u/%u checks passed\n", passed,
		   static_cast<unsigned>(CHECK_COUNT));
	cock::user::exit_process(passed == CHECK_COUNT ? EXIT_ALL_PASSED
												: EXIT_SOME_FAILED);
}
