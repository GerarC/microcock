#ifndef PROBE_PROBE_HPP
#define PROBE_PROBE_HPP

#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

namespace probe {

constexpr size_t PAGE_BYTES = 0x1000;
constexpr size_t PROBE_LENGTH = 16;
constexpr int NO_FILE = -1;
constexpr int MAPPING_PROTECTION = PROT_READ | PROT_WRITE;
constexpr uintptr_t KERNEL_ADDRESS = 0xC0100000;
constexpr uintptr_t UNMAPPED_USER_ADDRESS = 0x30000000;
constexpr size_t OVERSIZED_MAPPING = 0x20000000;
constexpr uint32_t PING_VALUE = 0x5EED;
constexpr uint8_t FIRST_PATTERN = 0xA5;
constexpr uint8_t ZERO_PATTERN = 0;
constexpr size_t NO_PAGES = 0;

bool portsAreDenied();
bool irqIsDenied();
bool grantIsDenied();
bool physicalVideoMapIsDenied();
bool physicalRamMapIsDenied();
bool kernelWriteIsDenied();
bool unmappedWriteIsDenied();
bool kernelUnmapIsDenied();
bool bootModulesAreDenied();

bool anonymousMemoryWorks();
bool mappingQuotaIsEnforced();
bool releasedAddressIsReused();
bool regionTableIsBounded();

bool irqNotificationIsRefused();
bool capabilityGrantIsRefused();
bool messageToSelfArrives();

bool oversizedWriteIsCut();
bool printfFormatsAndLongLines();

bool mallocReusesAndCoalesces();
bool reallocKeepsContents();
bool callocZeroesRecycledMemory();
bool hugeAllocationFailsCleanly();
bool operatorNewRoundTrips();

bool sharedMemoryRoundTrips();
bool sharedMemoryRefusesBadRequests();
bool sharedMemoryTableIsBounded();

bool initrdFileReads();
bool fileServerRefusesBadRequests();
bool moduleIndexIsDenied();

} // namespace probe

#endif // !PROBE_PROBE_HPP
