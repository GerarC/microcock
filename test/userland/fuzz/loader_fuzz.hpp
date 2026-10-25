/**
 * @file loader_fuzz.hpp
 * @brief Fuzzing of the program loader: corrupt ELF images given to `spawn` and to `exec`.
 *
 * Every image is the seed of `elf_mutator.hpp` with a few corruptions. The first one of each run is not
 * corrupted, so a loader that refuses everything does not pass. A run fails when the parser accepts an image
 * that breaks a promise of `imageIsSafe`, or when the valid image does not run.
 */
#ifndef FUZZ_LOADER_FUZZ_HPP
#define FUZZ_LOADER_FUZZ_HPP

#include <stdint.h>

namespace fuzz {

/** Counters of the loader runs; printed by the fuzzer. */
struct LoaderStatistics {
	/** Images built for `spawn`. */
	uint32_t spawnMutants;
	/** Images the parser accepted. */
	uint32_t spawnParsed;
	/** Images the parser accepted that were not safe. */
	uint32_t spawnUnsafe;
	/** Processes `spawn` started. */
	uint32_t spawnStarted;
	/** Copies of the fuzzer made to try `exec`. */
	uint32_t execChildren;
	/** Copies that `pm` could not make (no room, or refused). */
	uint32_t execForkFailed;
	/** Copies whose status `pm` did not give back. */
	uint32_t execLost;
};

/**
 * @brief Gives the counters of the loader runs so far.
 * @return Reference to the global counters.
 */
const LoaderStatistics &loaderStatistics();

/**
 * @brief Starts processes from corrupt images with `cock::user::spawn`.
 *
 * Needs the `SPAWN` capability. The processes it starts run the corrupt code with no rights and are left to
 * end by themselves.
 * @param seed Seed of the generator.
 * @param mutants Number of corrupt images to try, after the valid one.
 * @return True when the valid image started, `spawn` never started a process the local parser refused, and
 * the parser never accepted an unsafe image.
 */
bool runSpawnFuzz(uint32_t seed, uint32_t mutants);

/**
 * @brief Replaces the image of copies of the caller with corrupt images through `pm`.
 *
 * Each copy is made with `fork`, tries a few images (the first copy begins with the valid one), and ends by
 * itself when `pm` refuses them all; a copy that was replaced runs the corrupt code and is killed after a
 * short time. The caller collects every copy with `wait`.
 * @param manager Handle on the endpoint of `pm`.
 * @param seed Seed of the generator.
 * @param children Number of copies to make.
 * @return True when at least one copy was made, `pm` gave back the status of every copy, and the valid image
 * ran and ended with status 0.
 */
bool runExecFuzz(uint32_t manager, uint32_t seed, uint32_t children);

} // namespace fuzz

#endif // !FUZZ_LOADER_FUZZ_HPP
