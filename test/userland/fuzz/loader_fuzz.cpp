#include "loader_fuzz.hpp"
#include "elf_mutator.hpp"
#include "prng.hpp"
#include <cock/abi/executable.hpp>
#include <cock/abi/process.hpp>
#include <cock/elf/image.hpp>
#include <cock/user/api.hpp>
#include <cock/user/pm.hpp>
#include <cock/user/spawn.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscalls.h>

namespace fuzz {

static_assert(SEED_EXIT_CALL == SYS_EXIT, "the seed image must call the exit system call");

namespace {

constexpr uint32_t IMAGE_LIMIT =
	cock::abi::executable::USER_SPACE_LIMIT - static_cast<uint32_t>(cock::abi::process::PENDING_STACK_BYTES);
constexpr char SPAWN_NAME[] = "fuzzspawn";
constexpr char EXEC_NAME[] = "fuzzexec";
constexpr uint32_t EXEC_ATTEMPTS = 8;
constexpr uint32_t EXEC_CHILD_MILLISECONDS = 150;
constexpr uint32_t ONE_ARGUMENT = 1;
constexpr uintptr_t CHILD_ENDED = 0;
constexpr uint32_t FIRST_CHILD = 0;
constexpr uint32_t VALID_EXIT_STATUS = 0;

LoaderStatistics statistics;

[[noreturn]] void execChild(cock::user::pm::Client &manager, uint32_t seed, bool firstIsValid) {
	Prng prng(seed);
	const char *arguments[] = {EXEC_NAME};

	for (uint32_t attempt = 0; attempt < EXEC_ATTEMPTS; attempt++) {
		uint8_t image[MUTANT_CAPACITY];
		size_t size = buildSeedImage(image);
		if (!(firstIsValid && attempt == 0)) size = mutateImage(prng, image, size);

		uint8_t *area = manager.begin(size);
		if (area == nullptr) continue;

		memcpy(area, image, size);
		manager.finishExec(EXEC_NAME, arguments, ONE_ARGUMENT, size);
	}
	cock::user::exit_process(CHILD_ENDED);
}

} // namespace

const LoaderStatistics &loaderStatistics() { return statistics; }

bool runSpawnFuzz(uint32_t seed, uint32_t mutants) {
	Prng prng(seed);
	bool validStarted = false;

	for (uint32_t round = 0; round <= mutants; round++) {
		uint8_t image[MUTANT_CAPACITY];
		size_t size = buildSeedImage(image);
		if (round != 0) size = mutateImage(prng, image, size);
		statistics.spawnMutants++;

		cock::elf::Image parsed;
		bool accepted = cock::elf::parse(image, size, IMAGE_LIMIT, parsed);
		if (accepted) {
			statistics.spawnParsed++;
			if (!imageIsSafe(parsed, size, IMAGE_LIMIT)) statistics.spawnUnsafe++;
		}

		uint32_t pid = cock::user::spawn(image, size, SPAWN_NAME);
		if (pid != cock::user::NO_PROCESS) {
			statistics.spawnStarted++;
			if (round == 0) validStarted = true;
			if (!accepted) statistics.spawnUnsafe++;
		}
	}
	return validStarted && statistics.spawnUnsafe == 0;
}

bool runExecFuzz(uint32_t manager, uint32_t seed, uint32_t children) {
	cock::user::pm::Client client(manager);
	bool validRan = false;

	for (uint32_t index = 0; index < children; index++) {
		cock::user::pm::Forked forked = client.fork();
		if (forked.isChild()) execChild(client, seed + index, index == FIRST_CHILD);
		if (forked.status != cock::abi::pm::Status::OK) {
			statistics.execForkFailed++;
			continue;
		}

		statistics.execChildren++;
		cock::user::sleep_ms(EXEC_CHILD_MILLISECONDS);
		client.kill(forked.pid, cock::abi::process::SIGNAL_KILL);
		cock::user::pm::Waited waited = client.wait(forked.pid);
		if (waited.status != cock::abi::pm::Status::OK) {
			statistics.execLost++;
			continue;
		}
		if (index == FIRST_CHILD) validRan = waited.exitStatus == VALID_EXIT_STATUS;
	}
	return statistics.execChildren != 0 && statistics.execLost == 0 && validRan;
}

} // namespace fuzz
