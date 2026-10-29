#include <test/host/check.hpp>
#include <test/userland/fuzz/prng.hpp>
#include <stddef.h>
#include <stdint.h>

namespace host_test {

using fuzz::Prng;

namespace {

constexpr uint32_t SEED = 7;
constexpr uint32_t OTHER_SEED = 8;
constexpr size_t SAMPLE_COUNT = 64;
constexpr uint32_t LIMIT = 10;
constexpr size_t DRAWS = 5000;
constexpr uint32_t HALF = 50;
constexpr uint32_t NEVER = 0;
constexpr uint32_t ALWAYS = 100;
constexpr uint32_t SPREAD_MARGIN = 5;
constexpr uint32_t CHOICES[] = {11, 22, 33};

void checkSameSeedSameSequence() {
	Prng first(SEED);
	Prng second(SEED);
	bool same = true;
	for (size_t index = 0; index < SAMPLE_COUNT; index++)
		same = same && first.next() == second.next();
	CHECK_TRUE(same);
}

void checkDifferentSeedsDiffer() {
	Prng first(SEED);
	Prng second(OTHER_SEED);
	bool differs = false;
	for (size_t index = 0; index < SAMPLE_COUNT; index++)
		differs = differs || first.next() != second.next();
	CHECK_TRUE(differs);
}

void checkZeroSeedStillMoves() {
	Prng zero(0);
	uint32_t first = zero.next();
	CHECK_TRUE(first != 0);
	CHECK_TRUE(zero.next() != first);
}

void checkBelowStaysInRange() {
	Prng prng(SEED);
	bool inRange = true;
	for (size_t index = 0; index < DRAWS; index++)
		inRange = inRange && prng.below(LIMIT) < LIMIT;
	CHECK_TRUE(inRange);
	CHECK_EQ(static_cast<uint32_t>(0), prng.below(0));
}

void checkChance() {
	Prng prng(SEED);
	bool never = false;
	bool always = true;
	for (size_t index = 0; index < DRAWS; index++) {
		never = never || prng.chance(NEVER);
		always = always && prng.chance(ALWAYS);
	}
	CHECK_FALSE(never);
	CHECK_TRUE(always);

	size_t hits = 0;
	for (size_t index = 0; index < DRAWS; index++)
		if (prng.chance(HALF)) hits++;
	CHECK_TRUE(hits > DRAWS * (HALF - SPREAD_MARGIN) / ALWAYS);
	CHECK_TRUE(hits < DRAWS * (HALF + SPREAD_MARGIN) / ALWAYS);
}

void checkPickUsesEveryChoice() {
	Prng prng(SEED);
	bool seen[sizeof(CHOICES) / sizeof(CHOICES[0])] = {};
	for (size_t index = 0; index < DRAWS; index++) {
		uint32_t value = prng.pick(CHOICES);
		for (size_t choice = 0; choice < sizeof(seen); choice++)
			if (CHOICES[choice] == value) seen[choice] = true;
	}
	bool all = true;
	for (bool one : seen)
		all = all && one;
	CHECK_TRUE(all);
}

} // namespace

void testPrng() {
	checkSameSeedSameSequence();
	checkDifferentSeedsDiffer();
	checkZeroSeedStillMoves();
	checkBelowStaysInRange();
	checkChance();
	checkPickUsesEveryChoice();
}

} // namespace host_test
