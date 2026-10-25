/**
 * @file exec.hpp
 * @brief What a program that was started by `exec` receives: one page just below its stack with its
 * arguments and the handle it was born with.
 *
 * A program started by `spawn` gets one number in `EAX` at its entry: its initial handle, or zero. A
 * program started by `exec` keeps the capabilities of the program it replaces, so it keeps the initial
 * handle too, and it also gets arguments. The kernel passes one number only, so `pm` maps the page of
 * an `ArgsBlock` at `ARGS_ADDRESS` in the new image and the number is that address; no handle has that
 * value, so the start code tells the two cases apart.
 */
#ifndef SHARED_COCK_ABI_EXEC_HPP
#define SHARED_COCK_ABI_EXEC_HPP

#include <cock/abi/executable.hpp>
#include <cock/abi/process.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::exec {

/** Size in bytes of the page that holds the arguments. */
constexpr size_t ARGS_PAGE_BYTES = process::PENDING_PAGE_BYTES;
/** Address of the argument page in the image of a program started by `exec`: the page right below its stack. */
constexpr uint32_t ARGS_ADDRESS =
	executable::USER_SPACE_LIMIT - static_cast<uint32_t>(process::PENDING_STACK_BYTES) -
	static_cast<uint32_t>(ARGS_PAGE_BYTES);
/** First address a program image started by `exec` may not use: the argument page comes first. */
constexpr uint32_t IMAGE_LIMIT = ARGS_ADDRESS;
/** Most arguments, the program name included. */
constexpr uint32_t MAX_ARGS = 16;
/** Most bytes of the argument strings in all, terminators included. */
constexpr size_t MAX_ARG_BYTES = 768;
/** Value of `ArgsBlock::magic`, so the start code does not trust a page it did not expect. */
constexpr uint32_t ARGS_MAGIC = 0x45584543;
/** Value of `ArgsBlock::initialHandle` when the replaced program had none. */
constexpr uint32_t NO_INITIAL_HANDLE = 0;

/** Contents of the argument page, from its first byte. */
struct ArgsBlock {
	/** `ARGS_MAGIC`. */
	uint32_t magic;
	/** The initial handle of the program that was replaced, handed on unchanged. */
	uint32_t initialHandle;
	/** Number of arguments, at most `MAX_ARGS`; the first is the name the program was started with. */
	uint32_t count;
	/** Bytes used in `strings`. */
	uint32_t bytes;
	/** Start of each argument in `strings`, in order. */
	uint32_t offsets[MAX_ARGS];
	/** The arguments, each ending with a zero byte. */
	char strings[MAX_ARG_BYTES];
};

static_assert(sizeof(ArgsBlock) <= ARGS_PAGE_BYTES, "the arguments must fit in their page");
static_assert(ARGS_ADDRESS % ARGS_PAGE_BYTES == 0, "the argument page must be page aligned");

} // namespace cock::abi::exec

#endif // !SHARED_COCK_ABI_EXEC_HPP
