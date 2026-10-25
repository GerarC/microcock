/**
 * @file elf_mutator.hpp
 * @brief Builds a small valid ELF image and corrupts it, for the fuzzing of `spawn` and `exec`.
 *
 * The mutations aim at the fields the loader trusts: the program header table (offset, count), each segment
 * (offset, sizes, address) and the size of the image itself. `imageIsSafe` says whether an image that the
 * parser accepted keeps every promise the loader relies on, so a parser that lost a bounds check is caught
 * without running the image. Host tested in `test/host/test_elf_mutator.cpp`.
 */
#ifndef FUZZ_ELF_MUTATOR_HPP
#define FUZZ_ELF_MUTATOR_HPP

#include "prng.hpp"
#include <cock/elf/image.hpp>
#include <stddef.h>
#include <stdint.h>

namespace fuzz {

/** Address where the code segment of the seed image loads. */
constexpr uint32_t SEED_CODE_ADDRESS = 0x00400000;
/** Address where the data segment of the seed image loads; a different page from the code. */
constexpr uint32_t SEED_DATA_ADDRESS = 0x00402000;
/** Bytes of memory of the data segment of the seed image; it has no bytes in the file. */
constexpr uint32_t SEED_DATA_BYTES = 0x100;
/** Number of program headers of the seed image. */
constexpr size_t SEED_PROGRAM_HEADERS = 2;
/** Offset of the code in the seed image: right after the file header and the program header table. */
constexpr size_t SEED_CODE_OFFSET = cock::elf::HEADER_SIZE + SEED_PROGRAM_HEADERS * cock::elf::PROGRAM_HEADER_SIZE;
/** Number of the exit system call, the same as `SYS_EXIT` in `sys/syscalls.h` (checked there). */
constexpr uint8_t SEED_EXIT_CALL = 1;
/** Opcode of `mov eax, imm32`. */
constexpr uint8_t OPCODE_MOVE_EAX = 0xB8;
/** Opcode of `xor r/m32, r32`. */
constexpr uint8_t OPCODE_XOR = 0x31;
/** ModRM byte that makes the xor of `ebx` with itself. */
constexpr uint8_t MODRM_EBX_EBX = 0xDB;
/** Opcode of `int imm8`. */
constexpr uint8_t OPCODE_INTERRUPT = 0xCD;
/** Vector of the system call gate. */
constexpr uint8_t SYSCALL_VECTOR = 0x80;
/** A byte of zero, used to fill the rest of an immediate. */
constexpr uint8_t ZERO_BYTE = 0;
/** Size in bytes of the code of the seed image: the program calls exit with status 0. */
constexpr size_t SEED_CODE_BYTES = 9;
/** Size in bytes of the seed image. */
constexpr size_t SEED_IMAGE_BYTES = SEED_CODE_OFFSET + SEED_CODE_BYTES;
/** Bytes a mutation may add to the end of the image. */
constexpr size_t MUTANT_EXTRA_BYTES = 64;
/** Capacity every buffer given to `buildSeedImage` and `mutateImage` must have. */
constexpr size_t MUTANT_CAPACITY = SEED_IMAGE_BYTES + MUTANT_EXTRA_BYTES;

/**
 * @brief Writes the seed image: two segments (code, then data with nothing in the file) and a program that exits with status 0.
 * @param out Buffer of at least `MUTANT_CAPACITY` bytes.
 * @return The size of the image, `SEED_IMAGE_BYTES`.
 */
size_t buildSeedImage(uint8_t *out);

/**
 * @brief Applies one to three random corruptions to an image.
 *
 * Corruptions: flip a bit, set a byte, write a hostile word into the headers or anywhere, point a segment
 * offset outside the file, inflate a segment, cut or extend the image, copy a program header over another,
 * and change the count and the offset of the program header table.
 * @param prng Source of every choice; the same state gives the same mutant.
 * @param image The image to change in place; a buffer of `MUTANT_CAPACITY` bytes.
 * @param size Current size of the image.
 * @return The new size of the image, at most `MUTANT_CAPACITY`.
 */
size_t mutateImage(Prng &prng, uint8_t *image, size_t size);

/**
 * @brief Checks that an image the parser accepted is safe to load.
 *
 * Independent of the parser: it repeats the promises with 64 bit arithmetic.
 * @param image What `cock::elf::parse` returned.
 * @param size Size of the image that was parsed.
 * @param limit First address a segment may not reach.
 * @return True when there are at most `cock::elf::MAX_SEGMENTS` segments, every one has its file bytes inside the
 * image, a file size not above its memory size, a start above the reserved page and an end not above `limit`, no two
 * share a page, and the entry point is inside one of them.
 */
bool imageIsSafe(const cock::elf::Image &image, size_t size, uint32_t limit);

} // namespace fuzz

#endif // !FUZZ_ELF_MUTATOR_HPP
