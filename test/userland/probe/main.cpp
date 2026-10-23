#include "probe.hpp"
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace {

constexpr size_t CHECK_COUNT = 62;
constexpr uintptr_t EXIT_ALL_PASSED = 0;
constexpr uintptr_t EXIT_SOME_FAILED = 1;

} // namespace

extern "C" int main() {
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
		probe::mappingProtectionIsHonored(),
		probe::printfFormatsAndLongLines(),
		probe::mallocReusesAndCoalesces(),
		probe::reallocKeepsContents(),
		probe::callocZeroesRecycledMemory(),
		probe::hugeAllocationFailsCleanly(),
		probe::operatorNewRoundTrips(),
		probe::mallocIsSafeAcrossThreads(),
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
		probe::processAliveReportsLiveness(),
		probe::receiveTimeoutBehaves(),
		probe::timeoutCallsRefuseBadInput(),
		probe::sleepReturnsAndKeepsMessages(),
		probe::futexAndThreadCallsRefuseBadInput(),
		probe::threadsShareMemoryUnderMutex(),
		probe::futexWakesEveryWaiterAndThreadsAreBounded(),
		probe::futexLosesNoWakeup(),
		probe::synchronousCallsRoundTrip(),
		probe::queuedCallsAreServedInOrderAndMisuseIsRefused(),
		probe::deadServerFailsEveryCaller(),
		probe::callTimeoutExpiresAndLateReplyFails(),
		probe::acceptWaitsForMessagesAndTimesOut(),
		probe::deferredCallIsAnsweredAfterTheNextOne(),
		probe::staticConstructorsRun(),
		probe::joinWaitsForTheThread(),
		probe::conditionVariablePassesTheTurn(),
		probe::conditionVariableSignalsOneAndBroadcastsAll(),
		probe::semaphoreCountsAndBlocks(),
		probe::connectRefusesWhatThePolicyDoesNotList(),
		probe::unconnectedServersRefuseMessages(),
		probe::connectGivesTheRightToTalk(),
		probe::connectAcceptsPathsAndLongNames(),
		probe::connectWorksFromAnotherThread(),
		probe::registrationIsRootOnly(),
		probe::connectingTwiceUsesNoNewCapability(),
		probe::sharedMemoryNeedsConnect()};

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
