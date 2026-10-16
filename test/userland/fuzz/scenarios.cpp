#include "scenarios.hpp"
#include "prng.hpp"
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/thread.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::user::CALL_WORDS;
using cock::user::CallResult;
using cock::user::NO_PROCESS;

namespace fuzz {

namespace {

enum class Kind : uint32_t {
	CALLS,
	FUTEX,
};

struct Worker {
	Prng prng;
	uint32_t index;
};

constexpr size_t WORKERS = 3;
constexpr size_t PAGE_BYTES = 0x1000;
constexpr size_t STACK_BYTES = 0x4000;
constexpr size_t MAX_TICKETS = 4;
constexpr uint32_t STEPS = 50;
constexpr uint32_t WAIT_SLICE_MS = 20;
constexpr uint32_t WAIT_SLICES = 150;
constexpr uint32_t MAIN_STEP_MS = 2;
constexpr uint32_t CALL_ACTIONS = 8;
constexpr uint32_t SERVED_ACTIONS = 4;
constexpr uint32_t FUTEX_ACTIONS = 4;
constexpr uint32_t HOSTILE_PERCENT = 30;
constexpr uint32_t OTHER_WORKER_PERCENT = 80;
constexpr uint32_t WAKE_LIMIT = 4;
constexpr uint32_t EXPECTED_LIMIT = 3;
constexpr uint32_t STORM_TYPES = 40;
constexpr uint32_t STORM_REPEATS = 2;
constexpr uint32_t STORM_CALL_MS = 5;
constexpr uint32_t NOT_STARTED = 0;
constexpr uint32_t STARTED = 1;
constexpr uint32_t NO_FINISHED = 0;
constexpr uint32_t FUTEX_WORD_COUNT = 2;
constexpr uint32_t SEED_STRIDE = 7919;
constexpr uint32_t UNALIGNED_WORD = 0x500001;
constexpr uint32_t BOUNDED_MS[] = {0, 1, 2, 5};
constexpr uint32_t CALL_MS[] = {1, 2, 5, 50, 50};
constexpr uint32_t HOSTILE_WORDS[] = {0x0, 0x1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF};
constexpr uint32_t HOSTILE_TARGETS[] = {0x0, 0x1, 0xFFFFFFFE, 0xFFFFFFFF};

alignas(PAGE_BYTES) uint8_t workerStacks[WORKERS][STACK_BYTES];
Worker workers[WORKERS] = {{Prng(1), 0}, {Prng(1), 1}, {Prng(1), 2}};
uint32_t workerTids[WORKERS];
uint32_t go = NOT_STARTED;
uint32_t finished = NO_FINISHED;
uint32_t futexWords[FUTEX_WORD_COUNT];
Kind kind = Kind::CALLS;
Prng mainPrng(1);
ScenarioStatistics statistics;

void count(uint32_t &counter) { __atomic_add_fetch(&counter, 1, __ATOMIC_SEQ_CST); }

void randomWords(Prng &prng, uint32_t (&words)[CALL_WORDS]) {
	for (uint32_t &word : words)
		word = prng.chance(HOSTILE_PERCENT) ? prng.pick(HOSTILE_WORDS) : prng.next();
}

uint32_t pickTarget(Prng &prng) {
	if (prng.chance(HOSTILE_PERCENT)) return prng.pick(HOSTILE_TARGETS);
	return workerTids[prng.below(WORKERS)];
}

void serve(Worker &self, uint32_t (&tickets)[MAX_TICKETS], size_t &ticketCount) {
	uint32_t words[CALL_WORDS] = {};
	randomWords(self.prng, words);

	switch (self.prng.below(SERVED_ACTIONS)) {
	case 0:
		if (cock::user::ipc_reply(words)) count(statistics.replied);
		break;
	case 1: {
		uint32_t ticket = cock::user::ipc_defer();
		if (ticket != cock::user::NO_TICKET && ticketCount < MAX_TICKETS) {
			tickets[ticketCount++] = ticket;
			count(statistics.deferred);
		}
		break;
	}
	case 2:
		if (cock::user::ipc_reply_wait(words, self.prng.pick(BOUNDED_MS)).ok)
			count(statistics.served);
		break;
	default:
		break;
	}
}

void callsWorker(Worker &self) {
	uint32_t tickets[MAX_TICKETS] = {};
	size_t ticketCount = 0;

	for (uint32_t step = 0; step < STEPS; step++) {
		uint32_t words[CALL_WORDS] = {};
		randomWords(self.prng, words);

		switch (self.prng.below(CALL_ACTIONS)) {
		case 0:
		case 1: {
			CallResult served = cock::user::ipc_accept(self.prng.pick(BOUNDED_MS));
			if (served.ok) {
				count(statistics.served);
				serve(self, tickets, ticketCount);
			}
			break;
		}
		case 2:
		case 3:
			if (cock::user::ipc_call(pickTarget(self.prng), words,
									 self.prng.pick(CALL_MS)).ok)
				count(statistics.answered);
			break;
		case 4:
			cock::user::ipc_reply(words);
			break;
		case 5:
			if (ticketCount > 0) {
				if (cock::user::ipc_reply_to(tickets[--ticketCount], words))
					count(statistics.repliedTo);
			} else {
				cock::user::ipc_reply_to(pickTarget(self.prng), words);
			}
			break;
		case 6: {
			CallResult served =
				cock::user::ipc_reply_wait(words, self.prng.pick(BOUNDED_MS));
			if (served.ok) serve(self, tickets, ticketCount);
			break;
		}
		default:
			cock::user::ipc_defer();
			cock::user::sleep_ms(self.prng.below(EXPECTED_LIMIT));
			break;
		}
	}

	while (ticketCount > 0 && self.prng.chance(OTHER_WORKER_PERCENT)) {
		uint32_t words[CALL_WORDS] = {};
		randomWords(self.prng, words);
		if (cock::user::ipc_reply_to(tickets[--ticketCount], words)) count(statistics.repliedTo);
	}
}

void futexWorker(Worker &self) {
	uint32_t *word = &futexWords[self.index % FUTEX_WORD_COUNT];

	for (uint32_t step = 0; step < STEPS; step++) {
		switch (self.prng.below(FUTEX_ACTIONS)) {
		case 0:
			cock::user::futex_wait(word, self.prng.below(EXPECTED_LIMIT),
								   self.prng.pick(BOUNDED_MS));
			break;
		case 1:
			cock::user::futex_wake(word, self.prng.below(WAKE_LIMIT));
			break;
		case 2:
			__atomic_add_fetch(word, 1, __ATOMIC_SEQ_CST);
			break;
		default:
			cock::user::futex_wait(reinterpret_cast<const uint32_t *>(
									   static_cast<uintptr_t>(UNALIGNED_WORD)),
								   0, self.prng.pick(BOUNDED_MS));
			break;
		}
	}
}

void finish() {
	__atomic_add_fetch(&finished, 1, __ATOMIC_SEQ_CST);
	cock::user::futex_wake(&finished, WORKERS);
}

void worker(void *argument) {
	Worker &self = workers[reinterpret_cast<size_t>(argument)];
	while (__atomic_load_n(&go, __ATOMIC_SEQ_CST) == NOT_STARTED)
		cock::user::futex_wait(&go, NOT_STARTED, WAIT_SLICE_MS);

	if (kind == Kind::CALLS) {
		callsWorker(self);
	} else {
		futexWorker(self);
	}
	finish();
}

void driveFromMain() {
	uint32_t words[CALL_WORDS] = {};
	randomWords(mainPrng, words);
	cock::user::ipc_call(pickTarget(mainPrng), words, mainPrng.pick(BOUNDED_MS));

	cock::ipc::Message message = {};
	message.type = cock::ipc::MessageType::SYS_PING;
	message.payload.args.arg0 = mainPrng.next();
	cock::user::ipc_send(workerTids[mainPrng.below(WORKERS)], message);
}

bool runScenario(Kind scenario, uint32_t seed) {
	kind = scenario;
	mainPrng = Prng(seed);
	for (size_t index = 0; index < WORKERS; index++)
		workers[index].prng = Prng(seed + SEED_STRIDE * static_cast<uint32_t>(index + 1));
	__atomic_store_n(&go, NOT_STARTED, __ATOMIC_SEQ_CST);
	__atomic_store_n(&finished, NO_FINISHED, __ATOMIC_SEQ_CST);
	for (uint32_t &word : futexWords)
		word = 0;

	uint32_t started = 0;
	for (size_t index = 0; index < WORKERS; index++) {
		workerTids[index] = cock::user::thread_start(
			worker, reinterpret_cast<void *>(index), workerStacks[index], STACK_BYTES);
		if (workerTids[index] != NO_PROCESS) started++;
	}
	__atomic_store_n(&go, STARTED, __ATOMIC_SEQ_CST);
	cock::user::futex_wake(&go, WORKERS);

	for (uint32_t slice = 0; slice < WAIT_SLICES; slice++) {
		uint32_t seen = __atomic_load_n(&finished, __ATOMIC_SEQ_CST);
		if (seen >= started) return true;
		if (kind == Kind::CALLS) driveFromMain();
		cock::user::futex_wait(&finished, seen, MAIN_STEP_MS);
	}
	return __atomic_load_n(&finished, __ATOMIC_SEQ_CST) >= started;
}

} // namespace

const ScenarioStatistics &scenarioStatistics() { return statistics; }

bool runCallScenario(uint32_t seed) { return runScenario(Kind::CALLS, seed); }

bool runFutexScenario(uint32_t seed) { return runScenario(Kind::FUTEX, seed); }

void runInboxStorm(const World &world, uint32_t seed) {
	Prng prng(seed);
	for (size_t server = 0; server < world.serverCount; server++) {
		for (uint32_t repeat = 0; repeat < STORM_REPEATS; repeat++) {
			for (uint32_t type = 0; type < STORM_TYPES; type++) {
				cock::ipc::Message message = {};
				message.type = static_cast<cock::ipc::MessageType>(type);
				for (size_t argument = 0; argument < cock::ipc::MAX_ARGUMENTS; argument++)
					message.payload.argv[argument] =
						prng.chance(HOSTILE_PERCENT) ? prng.pick(HOSTILE_WORDS) : prng.next();
				cock::user::ipc_send(world.servers[server], message);
			}
		}
	}

	for (size_t target = 0; target < world.callTargetCount; target++) {
		uint32_t words[CALL_WORDS] = {};
		randomWords(prng, words);
		cock::user::ipc_call(world.callTargets[target], words, STORM_CALL_MS);
	}
}

} // namespace fuzz
