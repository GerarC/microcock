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
/** A `write` whose source buffer is a kernel address is refused (the raw result is `-1`) instead of read. */
bool kernelWriteIsDenied();
/** A `write` whose source buffer is an unmapped user address is refused (the raw result is `-1`). */
bool unmappedWriteIsDenied();
/** Unmapping a kernel address is refused. */
bool kernelUnmapIsDenied();
/** Opening a boot data module without the capability fails, as do kernel or unmapped name pointers and bad name lengths. */
bool bootModulesAreDenied();
/** Spawning a process without the capability fails, as does each `SYS_PROC_*` call, with a kernel pointer or a handle the caller does not own. */
bool spawnIsDenied();

/** Anonymous memory can be mapped, starts zeroed, can be written and is unmapped; a second unmap of the same range fails. */
bool anonymousMemoryWorks();
/** A mapping above the per process quota is refused. */
bool mappingQuotaIsEnforced();
/** The address range of an unmapped region is handed out again. */
bool releasedAddressIsReused();
/** Creating many mappings ends in a clean refusal, not a failure of the kernel. */
bool regionTableIsBounded();
/** A read only mapping is readable, starts zeroed and refuses a kernel write into it, while a read write mapping accepts it; a protection of none or with unknown bits is refused. */
bool mappingProtectionIsHonored();

/** A user process cannot forge an `IRQ_NOTIFICATION` message: the kernel refuses to send it. */
bool irqNotificationIsRefused();
/** A user process cannot forge a `CAPABILITY_GRANTED` message. */
bool capabilityGrantIsRefused();
/** A message sent to the own process id is received with the right sender, type and payload. */
bool messageToSelfArrives();
/** `ipc_try_recv` returns `EMPTY` at once when the inbox is empty, returns the message when there is one and `EMPTY` again after it; kernel and unmapped buffer addresses fail. */
bool tryReceiveDoesNotBlock();
/** `process_alive` is true for the probe itself and for the file server found by name, and false for an unknown id and for `NO_PROCESS`. */
bool processAliveReportsLiveness();
/** `ipc_recv_timeout` reports `EMPTY` after a short wait and with a zero wait when nothing arrives, returns a queued message, and reports `EMPTY` again once it is taken. */
bool receiveTimeoutBehaves();
/** `ipc_recv_timeout` with a kernel or unmapped buffer, or with a time above the maximum, and `sleep` with a time above the maximum, fail with the raw `FAILURE` result. */
bool timeoutCallsRefuseBadInput();
/** `sleep_ms(0)` and a short `sleep_ms` return true, and a message queued before the sleep is still there afterwards (the elapsed time is not measured). */
bool sleepReturnsAndKeepsMessages();

/** Two threads each increment a shared counter three times under a futex mutex, sleeping while they hold it, and the total is exact. */
bool threadsShareMemoryUnderMutex();
/** Threads are started that block on a futex word until thread creation is refused (fewer than the per process limit succeed); one `futex_wake` then wakes exactly as many as were created. */
bool futexWakesEveryWaiterAndThreadsAreBounded();
/** Three threads wait on one futex word while the main thread sets it and wakes them, fifty rounds in lock step; each waiter pauses between reading the word and waiting, so the wake arrives while nobody sleeps and `futex_wait` must see the changed word; every waiter proceeds every round (none reaches the one second limit) and the acknowledgements add up to the exact total. */
bool futexLosesNoWakeup();
/** `futex_wait` and `futex_wake` fail on null, unaligned, kernel and unmapped words, and `futex_wait` on a time above the maximum; `futex_wait` returns `FUTEX_VALUE_CHANGED` for a different value and `FUTEX_TIMED_OUT` for a zero or short time, and `futex_wake` on a word without waiters returns zero; `thread_create_raw` fails for kernel or unmapped entry points, kernel or unmapped stacks and a stack below the minimum. */
bool futexAndThreadCallsRefuseBadInput();

/** An echo server thread answers fifty calls with the payload words changed as expected and the right peer id, and a final call is answered before the server thread ends; a call to it after it ended fails. */
bool synchronousCallsRoundTrip();
/** A call to an unknown thread, a call to the own thread and a reply without a request are refused; two callers that arrive while the server is still starting are served in arrival order and each gets its own marker back. */
bool queuedCallsAreServedInOrderAndMisuseIsRefused();
/** When the server thread ends without answering, two callers that were queued on it both fail instead of waiting forever. */
bool deadServerFailsEveryCaller();
/** A call with a time above the maximum fails with `FAILED`; a call with a zero or a short limit to a slow server returns `TIMED_OUT`, and the reply that the server sends later is refused. */
bool callTimeoutExpiresAndLateReplyFails();
/** `ipc_accept` times out after a short limit and with a zero limit when nothing arrives, then returns `MESSAGE_PENDING` when an asynchronous message arrives, and that message is read with `ipc_try_recv`. */
bool acceptWaitsForMessagesAndTimesOut();
/** `ipc_defer` without a request and `ipc_reply_to` with an unknown ticket are refused; a request postponed with `ipc_defer` is answered with `ipc_reply_to` after the server has answered the next request. */
bool deferredCallIsAnsweredAfterTheNextOne();

/** A global object whose constructor writes to a volatile variable has run before `main` starts: `_start` of the shared `crt0` calls the static constructors. */
bool staticConstructorsRun();

/** A `JoinableThread` that works 30 ms is not joined by a 10 ms wait, is joined by a long one after its function stored a value, cannot be joined twice or started twice before the join, is joined when it ended long before, and the join of an object never started or whose start was refused (stack below the minimum) fails. */
bool joinWaitsForTheThread();
/** Two threads pass a turn back and forth fifty times each with a `Mutex` and a `ConditionVariable` (wait in a loop, broadcast), and both are joined within three seconds with the exact number of turns taken; then a thread waits twenty times on a mutex whose `unlock` signals the condition variable, which places the signal between the release and the sleep, and it must not sleep forever: a wait that reads the wake counter after it releases the mutex loses that wakeup and fails here. */
bool conditionVariablePassesTheTurn();
/** Three threads wait for a token under a mutex: none passes before a token exists, one `signal` with one token lets exactly one pass, and a `broadcast` with two more tokens lets the other two pass. */
bool conditionVariableSignalsOneAndBroadcastsAll();
/** A `Semaphore` created with two units gives two `tryWait` and refuses the third, takes back one posted unit; three threads blocked in `wait` pass one by one as units are posted. */
bool semaphoreCountsAndBlocks();

/** A raw `SYS_WRITE` of 200 bytes is cut: it returns a positive count below 200, not a failure. */
bool oversizedWriteIsCut();
/** `printf` formats its conversions correctly and prints lines longer than its buffer completely. */
bool printfFormatsAndLongLines();

/** A freed block is handed out again for the same size, and after two neighbouring blocks are freed a request of twice the size is placed at the first block (the blocks merged). */
bool mallocReusesAndCoalesces();
/** `realloc(nullptr, n)` allocates, and growing a block to a larger size keeps the old bytes (shrinking is not checked). */
bool reallocKeepsContents();
/** `calloc` returns zeroed memory after a dirty block was freed, and returns null when the product of the sizes overflows. */
bool callocZeroesRecycledMemory();
/** `malloc` of 512 MiB and of `SIZE_MAX` return null, and a small allocation still succeeds afterwards. */
bool hugeAllocationFailsCleanly();
/** `new[]` and `new` allocate, keep the stored values, and `delete[]` and `delete` release them. */
bool operatorNewRoundTrips();
/** Three threads allocate, fill, check and free blocks of varied sizes at the same time and no block is ever overwritten. */
bool mallocIsSafeAcrossThreads();

/** A two page object is created, mapped writable and found zeroed, written, unmapped with `munmap`, mapped again read only and read back, shared with the own process, released, and cannot be mapped after the release. */
bool sharedMemoryRoundTrips();
/** Zero pages and 4096 pages are refused by create, an unknown object id by map, release and share, and an unknown target process by share. */
bool sharedMemoryRefusesBadRequests();
/** Creating objects until the table is full ends in a clean refusal, and releasing them frees the slots. */
bool sharedMemoryTableIsBounded();

/** A file of the initrd is opened through `vfs` (with and without a leading slash), its size and type match, the whole file and a read from an offset return the expected bytes, and a read at the end returns zero. */
bool initrdFileReads();
/** Malformed requests to the file server (missing file, empty or too long path, unsupported flags, write access on the read only initrd, unknown or closed handle, offset past the end) fail with the expected error status. */
bool fileServerRefusesBadRequests();
/** Listing the root returns the expected file and directory, listing a subdirectory returns its entry and then stops, a file inside it reads back, and `readdir` on a file or an unknown handle fails. */
bool fileServerListsDirectories();
/** The manifest in the initrd can be read through `vfs`, parses, and has an entry for the probe itself with its own path and no capabilities. */
bool manifestMatchesTheBoot();
/** Resolving a boot module index without the capability fails. */
bool moduleIndexIsDenied();

/** The name server refuses to connect the probe to the block server and to the clock server, which its `ipc=` list does not name (`DENIED`, no process id), answers `NOT_FOUND` for a name nobody has and `BAD_REQUEST` for a name that does not fit the wire. */
bool connectRefusesWhatThePolicyDoesNotList();
/** A message to each live process of the first pids that is not the probe is refused by the kernel for at least three of them: the servers the probe never connected to. */
bool unconnectedServersRefuseMessages();
/** After `connect` to the console server, which the `ipc=` list names, a synchronous call to it is admitted and answered (with the status for an unknown operation). */
bool connectGivesTheRightToTalk();
/** A path (`/run/console`) names the same server as the short name, and a name of more than 12 bytes (several parts) is looked up and reported as not found. */
bool connectAcceptsPathsAndLongNames();
/** A second thread of the probe connects to the console server and learns the same process id as the first thread. */
bool connectWorksFromAnotherThread();
/** `DEFINE`, `ALLOW` and `BIND` sent to the name server by the probe are refused with `NOT_ROOT`: only `rs` describes the system. */
bool registrationIsRootOnly();
/** Connecting to the same server forty times, more than the capability table holds, still succeeds: the name server gives the capability once. */
bool connectingTwiceUsesNoNewCapability();
/** Sharing a shared memory object with the console server works after `connect`. */
bool sharedMemoryNeedsConnect();
/** `connect` gives a handle on the console endpoint that reaches the server and stays the same on a second connect; a handle number nobody gave fails; creating or binding an endpoint without authority fails. */
bool connectGivesAHandleOnTheEndpoint();

} // namespace probe

#endif // !PROBE_PROBE_HPP
