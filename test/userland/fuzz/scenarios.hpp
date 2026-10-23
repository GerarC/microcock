/**
 * @file scenarios.hpp
 * @brief Multi thread scenarios of the syscall fuzzer, which single random calls cannot reach.
 *
 * The fuzzer runs one of them every 100 rounds. A scenario that does not finish in a few seconds is reported as
 * stuck and makes the final sanity check fail.
 */
#ifndef FUZZ_SCENARIOS_HPP
#define FUZZ_SCENARIOS_HPP

#include <stddef.h>
#include <stdint.h>

namespace fuzz {

/** Processes that the inbox storm may talk to. The arrays are owned by the caller and must outlive the call. */
struct World {
	/** Process ids of the servers that receive random messages. */
	const uint32_t *servers;
	/** Number of entries of `servers`. */
	size_t serverCount;
	/** Process ids that also receive random synchronous calls (servers that tolerate garbage output; the console is left out). */
	const uint32_t *callTargets;
	/** Number of entries of `callTargets`. */
	size_t callTargetCount;
};

/** Counters that prove the call scenario did work; printed at the end of a run. */
struct ScenarioStatistics {
	/** Requests accepted by the scenario servers. */
	uint32_t served;
	/** Requests answered with `ipc_reply`. */
	uint32_t replied;
	/** Requests postponed with `ipc_defer`. */
	uint32_t deferred;
	/** Deferred requests answered with `ipc_reply_to`. */
	uint32_t repliedTo;
	/** Calls whose caller got a reply. */
	uint32_t answered;
};

/**
 * @brief Gives the counters of all call scenarios run so far.
 * @return Reference to the global counters.
 */
const ScenarioStatistics &scenarioStatistics();
/**
 * @brief Several threads accept, reply, defer, reply-to and call each other and hostile ids with short timeouts while the main thread calls them and sends messages. Threads exit while serving or queued.
 * @param seed Seed of the generators of the scenario threads.
 * @return True when every thread finished in time; false when the scenario is stuck.
 */
bool runCallScenario(uint32_t seed);
/**
 * @brief Several threads wait and wake on futex words with varied expected values and timeouts, including an unaligned address.
 * @param seed Seed of the generators of the scenario threads.
 * @return True when every thread finished in time; false when the scenario is stuck.
 */
bool runFutexScenario(uint32_t seed);
/**
 * @brief Sends every message type (0 to 39) with random arguments to every server and a synchronous call with random words to each call target.
 * @param world Servers and call targets.
 * @param seed Seed of the generator.
 */
void runInboxStorm(const World &world, uint32_t seed);

} // namespace fuzz

#endif // !FUZZ_SCENARIOS_HPP
