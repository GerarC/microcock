#include "prng.hpp"
#include "scenarios.hpp"
#include "specs.hpp"
#include <cock/abi/process.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscalls.h>

using cock::user::NO_PROCESS;
using fuzz::Arg;
using fuzz::Prng;
using fuzz::Spec;

#ifdef FUZZ_SEED
constexpr uint32_t SEED = FUZZ_SEED;
#else
constexpr uint32_t SEED = 1;
#endif

#ifdef FUZZ_ROUNDS
constexpr uint32_t ROUNDS = FUZZ_ROUNDS;
#else
constexpr uint32_t ROUNDS = 2000;
#endif

#ifdef FUZZ_ROOT
constexpr const char MODE_NAME[] = "root";
#else
constexpr const char MODE_NAME[] = "user";
#endif

#ifdef FUZZ_TRACE
constexpr bool TRACE = true;
#else
constexpr bool TRACE = false;
#endif

namespace {

constexpr size_t PAGE_BYTES = 0x1000;
constexpr size_t SCRATCH_PAGES = 4;
constexpr size_t SCRATCH_BYTES = SCRATCH_PAGES * PAGE_BYTES;
constexpr size_t STACK_BYTES = 0x4000;
constexpr size_t STACK_POOL = 8;
constexpr size_t MAX_SERVERS = 12;
constexpr size_t MAX_REGIONS = 16;
constexpr size_t MAX_SHM_IDS = 32;
constexpr size_t MAX_DEAD_IDS = 8;
constexpr size_t NAME_BUFFER_BYTES = 64;
constexpr uint32_t HOSTILE_PERCENT = 30;
constexpr uint32_t HOSTILE_NUMBER_PERCENT = 4;
constexpr uint32_t PLAUSIBLE_LENGTH_LIMIT = 64;
constexpr uint32_t PLAUSIBLE_VALUE_LIMIT = 64;
constexpr uint32_t PLAUSIBLE_PAGES_LIMIT = 8;
constexpr uint32_t PLAUSIBLE_MAP_PAGES_LIMIT = 16;
constexpr uint32_t CHECKPOINT_INTERVAL = 256;
constexpr uint32_t CLEANUP_INTERVAL = 64;
constexpr uint32_t DRAIN_INTERVAL = 16;
constexpr uint32_t DRAIN_LIMIT = 4096;
constexpr uint32_t WORD_ALIGNMENT = 4;
constexpr uintptr_t FAILED = static_cast<uintptr_t>(-1);
constexpr uintptr_t EXIT_OK = 0;
constexpr uintptr_t EXIT_BROKEN = 1;
constexpr uint32_t SANITY_MESSAGE_VALUE = 0x46555A5A;
constexpr uint32_t SELF_MESSAGE_VALUE = 0x5E1F;
constexpr uint32_t SMALL_TEXT_LENGTH = 5;
constexpr uint32_t UNUSED = 0;
constexpr uint32_t MESSAGE_TYPE_LIMIT = 40;
constexpr uint32_t MESSAGE_ARGUMENTS = cock::ipc::MAX_ARGUMENTS;

constexpr const char TEXT_SAMPLE[] = "fuzz\n";
constexpr const char *SERVER_NAMES[] = {"vga_driver", "console", "ata", "rtc",
										"fat",        "vfs",     "ext", "kbd", "ramfs"};
constexpr size_t SERVER_NAME_COUNT = sizeof(SERVER_NAMES) / sizeof(SERVER_NAMES[0]);
constexpr const char *CONSOLE_NAME = "console";
constexpr const char *NAME_SAMPLES[] = {"init", "ramfs", "initrd", "vfs", "fuzz", "x",
										"vga_driver"};

constexpr uintptr_t HOSTILE_ADDRESSES[] = {
	0x0,        0x1,        0xFFF,      0x1000,     0x500000,   0x10000000,
	0xBFFFFFFF, 0xC0000000, 0xC0001000, 0xFFFFF000, 0xFFFFFFF0, 0xFFFFFFFF};
constexpr uint32_t HOSTILE_LENGTHS[] = {0x0,        0x1,        0x1000,     0xFFFF,
										0x7FFFFFFF, 0x80000000, 0xFFFFF000, 0xFFFFFFFE,
										0xFFFFFFFF};
constexpr uint32_t HOSTILE_VALUES[] = {0x0, 0x1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF};
constexpr uint32_t HOSTILE_PAGES[] = {0x0, 0x81, 0x1000, 0xFFFFFFFF};
constexpr uint32_t HOSTILE_IDS[] = {0x0, 0x1, 0xFFFFFFFE, 0xFFFFFFFF, 0x7FFFF000};
constexpr uint32_t SMALL_MILLISECONDS[] = {0, 1, 2, 5, 10};
constexpr uint32_t REFUSED_MILLISECONDS[] = {0x80000000, 0x80000001, 0xFFFFFFFE};
constexpr uint32_t FLAG_VALUES[] = {0x0,  0x1,    0x2,    0x3,
									MAP_ANONYMOUS, MAP_ANONYMOUS | MAP_FIXED,
									MAP_PHYS, MAP_PHYS | MAP_FIXED};
constexpr uint32_t FD_VALUES[] = {0, 1, 2, 3, 4, 0xFFFFFFFF};
constexpr uint32_t HOSTILE_PORTS[] = {0x0, 0x60, 0x3F8, 0xFFFF, 0x10000, 0xFFFFFFFF};
constexpr uintptr_t HOSTILE_ENTRIES[] = {0x0, 0x1, 0xC0000000, 0xFFFFFFFF, 0x500000};
constexpr uint32_t STACK_SIZES[] = {0x0, 0x1000, 0x2000, STACK_BYTES, 0xFFFFFFFF};
constexpr uint32_t PLAUSIBLE_STACK_SIZES[] = {0x2000, STACK_BYTES};
constexpr uint32_t SHM_SMALL_IDS = 40;
constexpr uint32_t SANITY_WAIT_MILLISECONDS = 10;
constexpr uint32_t SCENARIO_INTERVAL = 100;
constexpr uint32_t SCENARIO_KINDS = 3;
constexpr uint32_t SCENARIO_CALLS = 0;
constexpr uint32_t SCENARIO_FUTEX = 1;
constexpr int NO_FD = -1;

alignas(PAGE_BYTES) uint8_t scratch[SCRATCH_BYTES];
alignas(PAGE_BYTES) uint8_t stacks[STACK_POOL][STACK_BYTES];
char nameBuffer[NAME_BUFFER_BYTES];

constinit Prng prng(SEED);
uint32_t servers[MAX_SERVERS];
const char *serverLabels[MAX_SERVERS];
size_t serverCount = 0;
uint32_t callTargets[MAX_SERVERS];
size_t callTargetCount = 0;
uintptr_t regions[MAX_REGIONS];
size_t regionCount = 0;
uint32_t shmIds[MAX_SHM_IDS];
size_t shmCount = 0;
uint32_t deadIds[MAX_DEAD_IDS];
size_t deadCount = 0;
size_t nextStack = 0;
uint32_t pendingNameLength = 0;
uint32_t selfPid = 0;
uint32_t selfTid = 0;
uint32_t stuckScenarios = 0;
uint32_t succeeded[fuzz::SPEC_COUNT];
uint32_t failed[fuzz::SPEC_COUNT];

void exitingThread() { cock::user::exit_process(EXIT_OK); }

bool isOk(uintptr_t result) { return result != FAILED; }

uintptr_t pickRegionOrScratch() {
	if (regionCount == 0) return reinterpret_cast<uintptr_t>(scratch);
	return regions[prng.below(static_cast<uint32_t>(regionCount))];
}

uintptr_t plausibleAddress() {
	constexpr uint32_t KINDS = 4;
	switch (prng.below(KINDS)) {
	case 0:
		return reinterpret_cast<uintptr_t>(scratch) + prng.below(SCRATCH_BYTES / 2);
	case 1:
		return pickRegionOrScratch();
	case 2:
		return reinterpret_cast<uintptr_t>(scratch);
	default:
		return reinterpret_cast<uintptr_t>(&exitingThread);
	}
}

uintptr_t plausibleWritableAddress() {
	constexpr uint32_t KINDS = 3;
	switch (prng.below(KINDS)) {
	case 0:
		return reinterpret_cast<uintptr_t>(scratch) + prng.below(SCRATCH_BYTES / 2);
	case 1:
		return pickRegionOrScratch();
	default:
		return reinterpret_cast<uintptr_t>(scratch);
	}
}

uintptr_t makeAddress(bool writable) {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_ADDRESSES);
	return writable ? plausibleWritableAddress() : plausibleAddress();
}

uintptr_t makeLength() {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_LENGTHS);
	return prng.below(PLAUSIBLE_LENGTH_LIMIT);
}

uintptr_t makeMapLength() {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_LENGTHS);
	return (prng.below(PLAUSIBLE_MAP_PAGES_LIMIT) + 1) * PAGE_BYTES;
}

uintptr_t makeValue() {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_VALUES);
	return prng.below(PLAUSIBLE_VALUE_LIMIT);
}

uintptr_t makeId() {
	constexpr uint32_t KINDS = 5;
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_IDS);
	switch (prng.below(KINDS)) {
	case 0:
		return selfPid;
	case 1:
		return selfTid;
	case 2:
		return deadCount > 0 ? deadIds[prng.below(static_cast<uint32_t>(deadCount))] : selfTid;
	default:
		return serverCount > 0 ? servers[prng.below(static_cast<uint32_t>(serverCount))]
							   : selfPid;
	}
}

uintptr_t makeCallTarget() {
	if (callTargetCount == 0 || prng.chance(HOSTILE_PERCENT)) return makeId();
	return callTargets[prng.below(static_cast<uint32_t>(callTargetCount))];
}

uintptr_t makeMilliseconds() {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(REFUSED_MILLISECONDS);
	return prng.pick(SMALL_MILLISECONDS);
}

uintptr_t makeShmId() {
	if (shmCount > 0 && !prng.chance(HOSTILE_PERCENT))
		return shmIds[prng.below(static_cast<uint32_t>(shmCount))];
	return prng.below(SHM_SMALL_IDS);
}

uintptr_t makeFlags() {
	if (prng.chance(HOSTILE_PERCENT)) return prng.next();
	return prng.pick(FLAG_VALUES);
}

uintptr_t makePages() {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_PAGES);
	return prng.below(PLAUSIBLE_PAGES_LIMIT) + 1;
}

uintptr_t makeNamePointer() {
	pendingNameLength = 0;
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_ADDRESSES);

	const char *name = prng.pick(NAME_SAMPLES);
	pendingNameLength = static_cast<uint32_t>(strlen(name));
	memcpy(nameBuffer, name, pendingNameLength + 1);
	return reinterpret_cast<uintptr_t>(nameBuffer);
}

uintptr_t makeNameLength() {
	if (pendingNameLength != 0 && !prng.chance(HOSTILE_PERCENT)) return pendingNameLength;
	return prng.pick(HOSTILE_LENGTHS);
}

uintptr_t makeEntry() {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_ENTRIES);
	return reinterpret_cast<uintptr_t>(&exitingThread);
}

uintptr_t makeStackBase() {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_ADDRESSES);
	uintptr_t base = reinterpret_cast<uintptr_t>(stacks[nextStack]);
	nextStack = (nextStack + 1) % STACK_POOL;
	return base;
}

uintptr_t makeStackSize() {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(STACK_SIZES);
	return prng.pick(PLAUSIBLE_STACK_SIZES);
}

uintptr_t makeArgument(Arg kind) {
	switch (kind) {
	case Arg::NONE:
		return UNUSED;
	case Arg::ID:
		return makeId();
	case Arg::ADDRESS_IN:
		return makeAddress(false);
	case Arg::ADDRESS_OUT:
		return makeAddress(true);
	case Arg::ADDRESS_ANY:
		return prng.chance(HOSTILE_PERCENT) ? prng.pick(HOSTILE_ADDRESSES) : UNUSED;
	case Arg::ADDRESS_ALIGNED:
		return makeAddress(true) & ~static_cast<uintptr_t>(WORD_ALIGNMENT - 1);
	case Arg::REGION:
		if (regionCount > 0 && !prng.chance(HOSTILE_PERCENT))
			return regions[prng.below(static_cast<uint32_t>(regionCount))];
		return prng.chance(HOSTILE_PERCENT) ? prng.pick(HOSTILE_ADDRESSES) : UNUSED;
	case Arg::LENGTH:
		return makeLength();
	case Arg::MAP_LENGTH:
		return makeMapLength();
	case Arg::PAGES:
		return makePages();
	case Arg::FLAGS:
		return makeFlags();
	case Arg::VALUE:
		return makeValue();
	case Arg::MILLISECONDS:
		return makeMilliseconds();
	case Arg::SHM:
		return makeShmId();
	case Arg::NAME_POINTER:
		return makeNamePointer();
	case Arg::NAME_LENGTH:
		return makeNameLength();
	case Arg::FD:
		return prng.pick(FD_VALUES);
	case Arg::PORT:
		return prng.chance(HOSTILE_PERCENT) ? prng.pick(HOSTILE_PORTS) : prng.below(PLAUSIBLE_VALUE_LIMIT);
	case Arg::ENTRY:
		return makeEntry();
	case Arg::STACK_BASE:
		return makeStackBase();
	case Arg::STACK_SIZE:
		return makeStackSize();
	case Arg::TEXT_POINTER:
		return prng.chance(HOSTILE_PERCENT) ? prng.pick(HOSTILE_ADDRESSES)
											: reinterpret_cast<uintptr_t>(TEXT_SAMPLE);
	case Arg::TEXT_LENGTH:
		return prng.chance(HOSTILE_PERCENT) ? prng.pick(HOSTILE_LENGTHS) : SMALL_TEXT_LENGTH;
	}
	return UNUSED;
}

void remember(uintptr_t *list, size_t &count, size_t capacity, uintptr_t value) {
	if (count < capacity) {
		list[count++] = value;
		return;
	}
	list[prng.below(static_cast<uint32_t>(capacity))] = value;
}

void rememberRegion(uintptr_t address) {
	remember(regions, regionCount, MAX_REGIONS, address);
}

void rememberShm(uint32_t id) {
	if (shmCount < MAX_SHM_IDS) shmIds[shmCount++] = id;
}

void rememberDead(uint32_t id) {
	if (deadCount < MAX_DEAD_IDS) {
		deadIds[deadCount++] = id;
		return;
	}
	deadIds[prng.below(MAX_DEAD_IDS)] = id;
}

void sendToSelf(uint32_t marker) {
	cock::ipc::Message message = {};
	message.type = cock::ipc::MessageType::SYS_PING;
	message.payload.args.arg0 = marker;
	cock::user::ipc_send(selfPid, message);
}

void fillOutgoingMessage() {
	cock::ipc::Message message = {};
	message.sender_pid = prng.next();
	message.target_pid = prng.next();
	message.type = static_cast<cock::ipc::MessageType>(prng.below(MESSAGE_TYPE_LIMIT));
	for (size_t index = 0; index < MESSAGE_ARGUMENTS; index++)
		message.payload.argv[index] = prng.next();
	memcpy(scratch, &message, sizeof(message));
}

void drainInbox() {
	cock::ipc::Message message = {};
	for (uint32_t count = 0; count < DRAIN_LIMIT; count++) {
		if (cock::user::ipc_try_recv(message) != cock::user::Received::MESSAGE) return;
	}
}

void releaseResources() {
	for (size_t index = 0; index < shmCount; index++)
		cock::user::shm_release(shmIds[index]);
	shmCount = 0;

	for (size_t index = 0; index < regionCount; index++) {
		for (uint32_t pages = 1; pages <= PLAUSIBLE_MAP_PAGES_LIMIT; pages++)
			syscall(SYS_MUNMAP, regions[index], PAGE_BYTES * pages, 0, 0, 0, 0);
	}
	regionCount = 0;
}

void afterCall(const Spec &spec, const uintptr_t (&args)[fuzz::ARGUMENT_COUNT],
			   uintptr_t result) {
	if (!isOk(result)) return;

	switch (spec.number) {
	case SYS_MMAP:
		if (args[3] & MAP_PHYS) {
			syscall(SYS_MUNMAP, result, args[1], 0, 0, 0, 0);
		} else {
			rememberRegion(result);
		}
		break;
	case SYS_SHM_MAP:
		rememberRegion(result);
		break;
	case SYS_SHM_CREATE:
	case SYS_MODULE_OPEN:
		rememberShm(static_cast<uint32_t>(result));
		break;
	case SYS_THREAD_CREATE:
		rememberDead(static_cast<uint32_t>(result));
		break;
	default:
		break;
	}
}

const Spec &chooseSpec() {
	static uint32_t totalWeight = 0;
	if (totalWeight == 0) {
		for (const Spec &spec : fuzz::SPECS)
			totalWeight += spec.weight;
	}

	uint32_t ticket = prng.below(totalWeight);
	for (const Spec &spec : fuzz::SPECS) {
		if (ticket < spec.weight) return spec;
		ticket -= spec.weight;
	}
	return fuzz::SPECS[0];
}

size_t indexOf(const Spec &spec) { return static_cast<size_t>(&spec - fuzz::SPECS); }

void fuzzOnce() {
	Spec hostile = {prng.next(), 1, {}};
	bool useHostileNumber = prng.chance(HOSTILE_NUMBER_PERCENT);
	const Spec &chosen = chooseSpec();
	const Spec &spec = useHostileNumber ? hostile : chosen;

	uintptr_t args[fuzz::ARGUMENT_COUNT] = {};
	for (size_t index = 0; index < fuzz::ARGUMENT_COUNT; index++)
		args[index] = makeArgument(spec.args[index]);
	if (useHostileNumber) {
		for (size_t index = 0; index < fuzz::ARGUMENT_COUNT; index++)
			args[index] = prng.next();
	}

	if (spec.number == SYS_IPC_RECV) sendToSelf(SELF_MESSAGE_VALUE);
	if (spec.number == SYS_IPC_SEND) fillOutgoingMessage();
	if (spec.number == SYS_IPC_CALL) args[0] = makeCallTarget();
	if (spec.number == SYS_IPC_SEND) args[1] = reinterpret_cast<uintptr_t>(scratch);

	if (TRACE) {
		printf("fuzz: pick %x hostile %u number %x\n", chosen.number,
			   static_cast<unsigned>(useHostileNumber), spec.number);
		printf("fuzz: call %x %x %x %x %x %x %x\n", spec.number,
			   static_cast<unsigned>(args[0]), static_cast<unsigned>(args[1]),
			   static_cast<unsigned>(args[2]), static_cast<unsigned>(args[3]),
			   static_cast<unsigned>(args[4]), static_cast<unsigned>(args[5]));
	}

	uintptr_t result = syscall(spec.number, args[0], args[1], args[2], args[3], args[4], args[5]);
	afterCall(spec, args, result);
	if (!useHostileNumber) {
		size_t slot = indexOf(chosen);
		if (isOk(result)) {
			succeeded[slot]++;
		} else {
			failed[slot]++;
		}
	}
}

void printStatistics() {
	const fuzz::ScenarioStatistics &scenario = fuzz::scenarioStatistics();
	printf("fuzz: scenario served %u replied %u deferred %u replied-to %u answered %u\n",
		   scenario.served, scenario.replied, scenario.deferred, scenario.repliedTo,
		   scenario.answered);
	for (size_t index = 0; index < fuzz::SPEC_COUNT; index++) {
		printf("fuzz: call %x ok %u failed %u\n", fuzz::SPECS[index].number, succeeded[index],
			   failed[index]);
	}
}

void collectServers() {
	for (size_t index = 0; index < SERVER_NAME_COUNT && serverCount < MAX_SERVERS; index++) {
		uint32_t pid = cock::user::lookup(SERVER_NAMES[index]);
		if (pid == NO_PROCESS) continue;

		serverLabels[serverCount] = SERVER_NAMES[index];
		servers[serverCount++] = pid;
		if (strcmp(SERVER_NAMES[index], CONSOLE_NAME) != 0 && pid != selfPid)
			callTargets[callTargetCount++] = pid;
	}
}

void runScenario(uint32_t round) {
	uint32_t seed = SEED + round;
	bool completed = true;
	switch ((round / SCENARIO_INTERVAL) % SCENARIO_KINDS) {
	case SCENARIO_CALLS:
		completed = fuzz::runCallScenario(seed);
		break;
	case SCENARIO_FUTEX:
		completed = fuzz::runFutexScenario(seed);
		break;
	default: {
		fuzz::World world = {servers, serverCount, callTargets, callTargetCount};
		fuzz::runInboxStorm(world, seed);
		break;
	}
	}

	if (!completed) {
		stuckScenarios++;
		printf("fuzz: scenario stuck at round %u\n", round);
	}
}

bool sanityCheck() {
	bool healthy = stuckScenarios == 0;

	if (cock::user::self() != selfPid || cock::user::gettid() != selfTid) {
		printf("fuzz: sanity: identity changed\n");
		healthy = false;
	}

	drainInbox();
	sendToSelf(SANITY_MESSAGE_VALUE);
	cock::ipc::Message message = {};
	cock::user::Received received =
		cock::user::ipc_recv_timeout(message, SANITY_WAIT_MILLISECONDS);
	if (received != cock::user::Received::MESSAGE ||
		message.payload.args.arg0 != SANITY_MESSAGE_VALUE) {
		printf("fuzz: sanity: self message lost (%u)\n", static_cast<unsigned>(received));
		healthy = false;
	}

	void *mapped = mmap(nullptr, PAGE_BYTES, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, NO_FD, 0);
	if (mapped == MAP_FAILED || munmap(mapped, PAGE_BYTES) != 0) {
		printf("fuzz: sanity: mmap or munmap failed\n");
		healthy = false;
	}

	for (size_t index = 0; index < serverCount; index++) {
		if (!cock::user::process_alive(servers[index]))
			printf("fuzz: server %s died\n", serverLabels[index]);
	}
	return healthy;
}

} // namespace

extern "C" void _start() {
	selfPid = cock::user::self();
	selfTid = cock::user::gettid();
	collectServers();

	printf("fuzz: seed %u rounds %u mode %s\n", SEED, ROUNDS, MODE_NAME);
	printf("fuzz: main thread %u\n", selfTid);

	for (uint32_t round = 1; round <= ROUNDS; round++) {
		fuzzOnce();
		if (round % DRAIN_INTERVAL == 0) drainInbox();
		if (round % CLEANUP_INTERVAL == 0) releaseResources();
		if (round % SCENARIO_INTERVAL == 0) runScenario(round);
		if (round % CHECKPOINT_INTERVAL == 0) printf("fuzz: round %u\n", round);
	}

	releaseResources();
	printStatistics();
	bool healthy = sanityCheck();
	printf(healthy ? "fuzz: sanity ok\n" : "fuzz: sanity FAILED\n");
	printf("fuzz: done seed %u\n", SEED);
	cock::user::exit_process(healthy ? EXIT_OK : EXIT_BROKEN);
}
