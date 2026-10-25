/**
 * @file exec.hpp
 * @brief What a program that was started by `exec` receives: one page just below its stack with its
 * arguments, the handle it was born with and the descriptors of the program it replaces.
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

/** Number of descriptors the table of a process holds; the C library has the same limit (`static_assert` there). */
constexpr size_t DESCRIPTOR_COUNT = 32;
/** Value of a `slots` element for a descriptor that is not open. */
constexpr uint8_t NO_DESCRIPTION = 0xFF;
/** Value of `DescriptorRecord::kind` for a description that does not exist. */
constexpr uint32_t KIND_NONE = 0;
/** Value of `DescriptorRecord::kind` for the console. */
constexpr uint32_t KIND_CONSOLE = 1;
/** Value of `DescriptorRecord::kind` for a file served by `vfs`. */
constexpr uint32_t KIND_FILE = 2;
/** Value of `DescriptorRecord::kind` for an end of a pipe. */
constexpr uint32_t KIND_PIPE = 3;
/** Largest value of `DescriptorRecord::kind` that is defined. */
constexpr uint32_t LAST_KIND = KIND_PIPE;
/** Bit of `DescriptorRecord::flags`: reads are allowed. */
constexpr uint32_t FLAG_READABLE = 1;
/** Bit of `DescriptorRecord::flags`: writes are allowed. */
constexpr uint32_t FLAG_WRITABLE = 2;
/** Bit of `DescriptorRecord::flags`: every write goes to the end. */
constexpr uint32_t FLAG_APPEND = 4;
/** Every bit of `DescriptorRecord::flags` that is defined. */
constexpr uint32_t KNOWN_FLAGS = FLAG_READABLE | FLAG_WRITABLE | FLAG_APPEND;

/** One open file description of the program that was replaced, as the C library kept it. */
struct DescriptorRecord {
	/** `KIND_NONE`, or what the description is. */
	uint32_t kind;
	/** Handle at the server that holds the object, as the kind defines it. */
	uint32_t handle;
	/** Offset of the next read or write, for the kinds that have one. */
	uint32_t offset;
	/** Bits of `FLAG_READABLE`, `FLAG_WRITABLE` and `FLAG_APPEND`. */
	uint32_t flags;
};

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
	/** Non zero when the replaced program handed on its descriptors: `descriptions` and `slots` are then meaningful. */
	uint32_t hasDescriptors;
	/** The open file descriptions of the replaced program; a description is named by its index. */
	DescriptorRecord descriptions[DESCRIPTOR_COUNT];
	/** For each descriptor, the index of the description it names, or `NO_DESCRIPTION`; two descriptors with the same index share it. */
	uint8_t slots[DESCRIPTOR_COUNT];
};

static_assert(sizeof(ArgsBlock) <= ARGS_PAGE_BYTES, "the arguments must fit in their page");
static_assert(ARGS_ADDRESS % ARGS_PAGE_BYTES == 0, "the argument page must be page aligned");

} // namespace cock::abi::exec

#endif // !SHARED_COCK_ABI_EXEC_HPP
