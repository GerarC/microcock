#ifndef FUZZ_SCENARIOS_HPP
#define FUZZ_SCENARIOS_HPP

#include <stddef.h>
#include <stdint.h>

namespace fuzz {

struct World {
	const uint32_t *servers;
	size_t serverCount;
	const uint32_t *callTargets;
	size_t callTargetCount;
};

struct ScenarioStatistics {
	uint32_t served;
	uint32_t replied;
	uint32_t deferred;
	uint32_t repliedTo;
	uint32_t answered;
};

const ScenarioStatistics &scenarioStatistics();
bool runCallScenario(uint32_t seed);
bool runFutexScenario(uint32_t seed);
void runInboxStorm(const World &world, uint32_t seed);

} // namespace fuzz

#endif // !FUZZ_SCENARIOS_HPP
