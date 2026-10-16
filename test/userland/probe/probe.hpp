/**
 * @file probe.hpp
 * @brief Checks run by the `probe` program, an unprivileged Ring 3 process that exercises the kernel and the servers from outside.
 *
 * Every function returns true when the behavior under test was observed and false otherwise; `main.cpp` prints one
 * line per check and the QEMU script requires them. Functions named `...IsDenied` or `...IsRefused` are negative tests:
 * the probe has no capabilities, so the call must fail and the kernel must stay healthy.
 */
#ifndef PROBE_PROBE_HPP
#define PROBE_PROBE_HPP

#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

namespace probe {

/** Size of a page in bytes. */
constexpr size_t PAGE_BYTES = 0x1000;
/** Bytes of the scratch buffers used by the checks. */
constexpr size_t PROBE_LENGTH = 16;
/** File descriptor argument for anonymous mappings. */
constexpr int NO_FILE = -1;
/** Protection of the test mappings. */
constexpr int MAPPING_PROTECTION = PROT_READ | PROT_WRITE;
/** An address in the kernel half, which user calls must never be allowed to touch. */
constexpr uintptr_t KERNEL_ADDRESS = 0xC0100000;
/** A user address that nothing maps. */
constexpr uintptr_t UNMAPPED_USER_ADDRESS = 0x30000000;
/** A mapping size (512 MiB) larger than the quota. */
constexpr size_t OVERSIZED_MAPPING = 0x20000000;
/** Payload of the message that the probe sends to itself. */
constexpr uint32_t PING_VALUE = 0x5EED;
/** First byte pattern written to memory to detect leaks between allocations. */
constexpr uint8_t FIRST_PATTERN = 0xA5;
/** Byte value expected in zeroed memory. */
constexpr uint8_t ZERO_PATTERN = 0;
/** A page count of zero, which the kernel must refuse. */
constexpr size_t NO_PAGES = 0;

/** Requesting an I/O port range without the capability fails. */
bool portsAreDenied();
/** Binding an interrupt line without the capability fails. */
bool irqIsDenied();
/** Granting a capability from a process that is not the root server fails. */
bool grantIsDenied();
/** Mapping the video memory physical range without a capability fails. */
bool physicalVideoMapIsDenied();
/** Mapping a physical RAM range without a capability fails. */
bool physicalRamMapIsDenied();
/** Passing a kernel address as an output buffer to a call is refused instead of written. */
bool kernelWriteIsDenied();
/** Passing an unmapped user address as an output buffer is refused. */
bool unmappedWriteIsDenied();
/** Unmapping a kernel address is refused. */
bool kernelUnmapIsDenied();
/** Opening a boot data module without the capability fails, as do kernel or unmapped name pointers and bad name lengths. */
bool bootModulesAreDenied();
/** Spawning a process without the capability fails, as do kernel pointers and a zero length image. */
bool spawnIsDenied();

/** Anonymous memory can be mapped, starts zeroed, can be written and is unmapped; a second unmap of the same range fails. */
bool anonymousMemoryWorks();
/** A mapping above the per process quota is refused. */
bool mappingQuotaIsEnforced();
/** The address range of an unmapped region is handed out again. */
bool releasedAddressIsReused();
/** Creating many mappings ends in a clean refusal, not a failure of the kernel. */
bool regionTableIsBounded();

/** A user process cannot forge an `IRQ_NOTIFICATION` message: the kernel refuses to send it. */
bool irqNotificationIsRefused();
/** A user process cannot forge a `CAPABILITY_GRANTED` message. */
bool capabilityGrantIsRefused();
/** A message sent to the own process id is received with the right sender, type and payload. */
bool messageToSelfArrives();
/** `ipc_try_recv` returns at once when the inbox is empty and returns the message when there is one. */
bool tryReceiveDoesNotBlock();
/** `process_alive` is true for a live process and false for a dead or invalid one. */
bool processAliveReportsLiveness();
/** `ipc_recv_timeout` times out when nothing arrives and returns early when a message does. */
bool receiveTimeoutBehaves();
/** Timeouts above the maximum, and a bad message pointer, are refused by the timed calls. */
bool timeoutCallsRefuseBadInput();
/** `sleep` returns after about the requested time and does not lose queued messages. */
bool sleepReturnsAndKeepsMessages();

/** Several threads increment a shared counter under a futex mutex and the total is exact. */
bool threadsShareMemoryUnderMutex();
/** `futex_wake` wakes every waiter, and creating more threads than the per process limit fails cleanly. */
bool futexWakesEveryWaiterAndThreadsAreBounded();
/** Unaligned or invalid futex words, bad entry points and small stacks are refused. */
bool futexAndThreadCallsRefuseBadInput();

/** A synchronous call reaches a server thread and its reply comes back with the payload words. */
bool synchronousCallsRoundTrip();
/** Callers that arrive while the server is busy are served in order; replying without a request or to the wrong caller is refused. */
bool queuedCallsAreServedInOrderAndMisuseIsRefused();
/** When a server dies, every caller blocked on it fails instead of waiting forever. */
bool deadServerFailsEveryCaller();
/** A call with a time limit returns `TIMED_OUT`, and a reply that comes later fails. */
bool callTimeoutExpiresAndLateReplyFails();
/** `ipc_accept` returns `MESSAGE_PENDING` when an asynchronous message arrives and times out when nothing does. */
bool acceptWaitsForMessagesAndTimesOut();
/** A deferred request can be answered with `ipc_reply_to` after the server has served another request. */
bool deferredCallIsAnsweredAfterTheNextOne();

/** A `write` larger than the kernel limit is cut, not rejected or overflowed. */
bool oversizedWriteIsCut();
/** `printf` formats its conversions correctly and prints lines longer than its buffer completely. */
bool printfFormatsAndLongLines();

/** Freed blocks are reused and adjacent free blocks are merged. */
bool mallocReusesAndCoalesces();
/** `realloc` keeps the old contents when it grows or shrinks. */
bool reallocKeepsContents();
/** `calloc` returns zeroed memory even when it reuses a dirty block. */
bool callocZeroesRecycledMemory();
/** An impossible allocation returns null and the allocator still works afterwards. */
bool hugeAllocationFailsCleanly();
/** `operator new` and `operator delete` allocate, hold data and release. */
bool operatorNewRoundTrips();

/** A shared object can be created, mapped, written, read back, and released. */
bool sharedMemoryRoundTrips();
/** Zero pages, too many pages, an unknown object id and an unknown target process are refused by create, map, release and share. */
bool sharedMemoryRefusesBadRequests();
/** Creating objects until the table is full ends in a clean refusal, and releasing them frees the slots. */
bool sharedMemoryTableIsBounded();

/** A file of the initrd is opened through `vfs`, its size matches and its contents read back. */
bool initrdFileReads();
/** Malformed requests to the file server (bad path, handle, offset, flags) get the right error status. */
bool fileServerRefusesBadRequests();
/** Directory listing returns the expected entries and ends with `OUT_OF_RANGE`. */
bool fileServerListsDirectories();
/** The manifest in the initrd can be read through `vfs`, parses, and has an entry for the probe itself with its own path and no capabilities. */
bool manifestMatchesTheBoot();
/** Resolving a boot module index without the capability fails. */
bool moduleIndexIsDenied();

} // namespace probe

#endif // !PROBE_PROBE_HPP
