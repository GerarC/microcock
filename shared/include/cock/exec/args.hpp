/**
 * @file args.hpp
 * @brief Building, checking and reading the argument page of a program started by `exec`.
 *
 * The caller of `exec` builds an `ArgsBlock` with `build`, `pm` checks the block it received with
 * `valid` before it writes it into the new image, and the start code of the new program reads it with
 * `argument`. Nothing here allocates.
 */
#ifndef SHARED_COCK_EXEC_ARGS_HPP
#define SHARED_COCK_EXEC_ARGS_HPP

#include <cock/abi/exec.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::exec {

/**
 * @brief Fills an argument block.
 *
 * @param block Receives the block; every byte is written, so nothing of the caller's memory leaks.
 * @param initialHandle The initial handle to hand on.
 * @param argv The arguments, `count` strings that end with a zero byte.
 * @param count Number of arguments, at most `abi::exec::MAX_ARGS`.
 * @return False when there are too many arguments or their strings, terminators included, do not fit
 * in `abi::exec::MAX_ARG_BYTES`; the block is left empty then.
 */
inline bool build(abi::exec::ArgsBlock &block, uint32_t initialHandle, const char *const *argv,
				  uint32_t count) {
	uint8_t *bytes = reinterpret_cast<uint8_t *>(&block);
	for (size_t index = 0; index < sizeof(block); index++) bytes[index] = 0;
	if (count > abi::exec::MAX_ARGS) return false;
	block.magic = abi::exec::ARGS_MAGIC;
	block.initialHandle = initialHandle;

	uint32_t used = 0;
	for (uint32_t arg = 0; arg < count; arg++) {
		block.offsets[arg] = used;
		for (size_t position = 0;; position++) {
			if (used == abi::exec::MAX_ARG_BYTES) {
				for (size_t index = 0; index < sizeof(block); index++) bytes[index] = 0;
				return false;
			}
			char letter = argv[arg][position];
			block.strings[used++] = letter;
			if (letter == '\0') break;
		}
	}
	block.count = count;
	block.bytes = used;
	return true;
}

/**
 * @brief Tells whether a block is well formed, as `pm` must know before it hands it to a new program.
 * @param block A block received from a caller, not trusted.
 * @return True when the magic is right, the count and the byte count are within their limits, every
 * argument starts inside the used bytes, and the used bytes end with a zero, so every argument ends.
 */
inline bool valid(const abi::exec::ArgsBlock &block) {
	if (block.magic != abi::exec::ARGS_MAGIC || block.count > abi::exec::MAX_ARGS ||
		block.bytes > abi::exec::MAX_ARG_BYTES)
		return false;
	if (block.count == 0) return block.bytes == 0;
	if (block.bytes == 0 || block.strings[block.bytes - 1] != '\0') return false;
	for (uint32_t arg = 0; arg < block.count; arg++)
		if (block.offsets[arg] >= block.bytes) return false;
	return true;
}

/**
 * @brief Reads one argument.
 * @param block A block that passed `valid`.
 * @param index Position, from 0.
 * @return The string, or null when the index is past the last argument.
 */
inline const char *argument(const abi::exec::ArgsBlock &block, uint32_t index) {
	if (index >= block.count) return nullptr;
	return block.strings + block.offsets[index];
}

} // namespace cock::exec

#endif // !SHARED_COCK_EXEC_ARGS_HPP
