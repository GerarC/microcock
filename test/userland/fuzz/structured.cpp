#include "structured.hpp"
#include "file_fuzz.hpp"
#include "loader_fuzz.hpp"
#include <cock/abi/names.hpp>
#include <cock/user/names.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace fuzz {

namespace {

constexpr const char *FILE_SERVERS[] = {"vfs", "ramfs", "fat", "ext"};
constexpr char MANAGER_NAME[] = "pm";
constexpr uint32_t FILE_ROUNDS = 400;
constexpr uint32_t SPAWN_MUTANTS = 600;
constexpr uint32_t EXEC_CHILDREN = 24;

const char *outcomeName(FileFuzzOutcome outcome) {
	switch (outcome) {
	case FileFuzzOutcome::NOT_REACHABLE:
		return "not reachable";
	case FileFuzzOutcome::SURVIVED:
		return "survived";
	case FileFuzzOutcome::LOST:
		return "LOST";
	}
	return "unknown";
}

bool fuzzFileServers(uint32_t seed) {
	bool healthy = true;
	size_t reached = 0;

	for (const char *name : FILE_SERVERS) {
		FileFuzzReport report = runFileFuzz(name, seed, FILE_ROUNDS);
		printf("fuzz: fz1 files %s %s requests %u answered %u timed-out %u\n", name,
			   outcomeName(report.outcome), report.requests, report.answered, report.timedOut);
		if (report.outcome != FileFuzzOutcome::NOT_REACHABLE) reached++;
		if (report.outcome == FileFuzzOutcome::LOST) healthy = false;
	}
	return healthy && reached != 0;
}

bool fuzzExec(uint32_t seed) {
	cock::user::names::Connection manager = cock::user::names::connect(MANAGER_NAME);
	if (manager.status != cock::abi::names::Status::OK) {
		printf("fuzz: fz1 exec pm not reachable\n");
		return false;
	}

	bool passed = runExecFuzz(manager.handle, seed, EXEC_CHILDREN);
	const LoaderStatistics &statistics = loaderStatistics();
	printf("fuzz: fz1 exec %s children %u fork-failed %u lost %u\n", passed ? "ok" : "FAILED",
		   statistics.execChildren, statistics.execForkFailed, statistics.execLost);
	return passed;
}

bool fuzzSpawn(uint32_t seed) {
	bool passed = runSpawnFuzz(seed, SPAWN_MUTANTS);
	const LoaderStatistics &statistics = loaderStatistics();
	printf("fuzz: fz1 spawn %s mutants %u parsed %u started %u unsafe %u\n", passed ? "ok" : "FAILED",
		   statistics.spawnMutants, statistics.spawnParsed, statistics.spawnStarted, statistics.spawnUnsafe);
	return passed;
}

} // namespace

bool runStructuredPhases(uint32_t seed, bool rootMode) {
	bool healthy = true;
	if (rootMode) {
		healthy = fuzzSpawn(seed);
	} else {
		healthy = fuzzFileServers(seed);
		healthy = fuzzExec(seed) && healthy;
	}
	printf(healthy ? "fuzz: fz1 ok\n" : "fuzz: fz1 FAILED\n");
	return healthy;
}

} // namespace fuzz
