/**
 * @file structured.hpp
 * @brief The structured part of the fuzzer (FZ1): corrupt programs for the loader and hostile requests for the file servers.
 *
 * Run once after the random system calls. With every capability (`FUZZ=root`) the fuzzer is the first program and
 * has neither the name server's endpoints nor `pm`, so it starts processes from corrupt images with `spawn`. As an
 * ordinary program (`FUZZ=user`) it reaches `vfs`, `ramfs`, `fat`, `ext` and `pm` by name, so it sends hostile
 * requests to the file servers and replaces the image of copies of itself with corrupt images through `pm`.
 */
#ifndef FUZZ_STRUCTURED_HPP
#define FUZZ_STRUCTURED_HPP

#include <stdint.h>

namespace fuzz {

/**
 * @brief Runs the structured phases that fit the mode and prints one line for each and a verdict.
 *
 * The last line is `fuzz: fz1 ok` or `fuzz: fz1 FAILED`.
 * @param seed Seed of the generators of the phases.
 * @param rootMode True when the fuzzer replaced `rs` and holds every capability.
 * @return True when no phase failed: a file server kept answering, a valid image ran, and no unsafe image was
 * accepted. A server that cannot be reached is skipped, but at least one file server must be reached as a user.
 */
bool runStructuredPhases(uint32_t seed, bool rootMode);

} // namespace fuzz

#endif // !FUZZ_STRUCTURED_HPP
