/**
 * @file descriptor.hpp
 * @brief Table of descriptors of a process, kept by the C library.
 *
 * A descriptor is a small number that names an open file description. Several descriptors may name the same
 * description (`dup`, `dup2`), and then they share its offset, as POSIX says; the description is closed when its
 * last descriptor is. Each description has an `Operations` table, which is how `read`, `write`, `lseek` and
 * `close` reach the console, a file served by `vfs` or, later, a pipe, with no case analysis of the descriptor
 * number. Descriptors 0, 1 and 2 are born as the console. The table lives in the memory of the process, so a
 * `fork` copies it with the rest of the address space.
 */
#ifndef LIBC_UNISTD_DESCRIPTOR_HPP
#define LIBC_UNISTD_DESCRIPTOR_HPP

#include <cock/abi/exec.hpp>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

namespace cock::libc {

/** Descriptor of standard input. */
constexpr int STDIN_DESCRIPTOR = 0;
/** Descriptor of standard output. */
constexpr int STDOUT_DESCRIPTOR = 1;
/** Descriptor of standard error. */
constexpr int STDERR_DESCRIPTOR = 2;
/** Number of descriptors a process is born with. */
constexpr int STANDARD_DESCRIPTOR_COUNT = 3;
/** Most descriptors a process may hold, the standard ones included. */
constexpr int MAX_DESCRIPTORS = 32;
/** Value of `Description::handle` for a kind of description that has no handle at its server. */
constexpr uint32_t NO_HANDLE = 0;
/** Value that `close` of an operations table returns when it worked. */
constexpr int CLOSED = 0;
/** Return value of the descriptor calls that fail. */
constexpr int FAILURE = -1;

static_assert(MAX_DESCRIPTORS == cock::abi::exec::DESCRIPTOR_COUNT,
			  "the table the libc keeps and the one an exec hands on must have the same size");

struct Description;

/**
 * What one kind of description does. Every function receives the description it acts on. `read`, `write` and
 * `seek` return -1 and set `errno` when they fail, as the public calls do; the descriptor layer has already
 * checked the access mode, the buffer and the count for `read` and `write`.
 */
struct Operations {
	/** What the kind is, one of the `KIND_` values of `abi/exec.hpp`: how the table is handed on through `exec`. */
	uint32_t kind;
	/** Reads up to `count` bytes (more than zero) into `buffer`; returns the bytes read, 0 at the end. */
	ssize_t (*read)(Description &description, void *buffer, size_t count);
	/** Writes up to `count` bytes (more than zero) from `buffer`; returns the bytes written. */
	ssize_t (*write)(Description &description, const void *buffer, size_t count);
	/** Moves the offset as `lseek` does; returns the new offset. */
	off_t (*seek)(Description &description, off_t offset, int whence);
	/** Releases what the description holds at its server; returns `CLOSED` or an `errno` value. Called once, when the last descriptor closes. */
	int (*close)(Description &description);
};

/** One open file description: what is open, how, and where the next read or write happens. */
struct Description {
	/** The kind; null while the slot is free. */
	const Operations *operations;
	/** Number of descriptors that name it; the description is closed when it falls to zero. */
	uint32_t references;
	/** Handle at the server, or `NO_HANDLE`. */
	uint32_t handle;
	/** Offset of the next read or write, for the kinds that have one. */
	uint32_t offset;
	/** Reads are allowed. */
	bool readable;
	/** Writes are allowed. */
	bool writable;
	/** Every write goes to the end. */
	bool append;
};

/**
 * @brief Stores an error code for the caller of a libc call.
 * @param code The `errno` value.
 * @return `FAILURE`, so a call can `return failWith(EBADF);`.
 */
int failWith(int code);

/**
 * @brief Gives a new description to the lowest free descriptor.
 * @param operations The kind; it must outlive the description.
 * @param handle Handle at the server, or `NO_HANDLE`.
 * @param readable Reads are allowed.
 * @param writable Writes are allowed.
 * @param append Every write goes to the end.
 * @return The descriptor, or `FAILURE` with `errno` set to `EMFILE` when the table is full.
 */
int openDescription(const Operations &operations, uint32_t handle, bool readable, bool writable, bool append);

/**
 * @brief Finds the description of a descriptor.
 * @param descriptor The number.
 * @return The description, or null when the number is out of range or not open.
 */
Description *describe(int descriptor);

/**
 * @brief Forgets every description of one kind, with no call to the server.
 *
 * For a server that is gone: the handles it gave mean nothing any more. The descriptors that named them become
 * free.
 * @param operations The kind to drop.
 */
void dropDescriptions(const Operations &operations);

} // namespace cock::libc

extern "C" {

/**
 * @brief The operations of file descriptions, defined by `file.cpp`.
 * @return The table; a program that did not link `file.cpp` has no such function, and the table of descriptors
 * treats the kind as unknown.
 */
const cock::libc::Operations *__file_operations();

/**
 * @brief The operations of pipe ends, defined by `pipe.cpp`.
 * @return The table; absent in a program that did not link `pipe.cpp`.
 */
const cock::libc::Operations *__pipe_operations();

/**
 * @brief Tells `file.cpp` that the descriptors it holds came from an `exec`, so its first connection to `vfs` keeps
 * the files the process has open there instead of closing them as a new connection would.
 */
void __file_inherited();

/**
 * @brief Writes the table of descriptors into the argument block of an `exec`.
 *
 * Called by the process manager client when a program replaces its image, so the new image has the same
 * descriptors: every description with its kind, handle, offset and mode, and which descriptors share which.
 * The servers keep what the handles name, because the process keeps its process id.
 *
 * @param block The block `cock::exec::build` filled; its descriptor fields are set.
 */
void __descriptors_export(cock::abi::exec::ArgsBlock *block);

/**
 * @brief Replaces the table with the one an `exec` handed on.
 *
 * Called by the start code of a program started by `exec`, before the constructors run. A block that hands on no
 * descriptors leaves the table as it is born (the console on 0 to 2). A description of a kind this program did not
 * link is dropped, and so are the descriptors that named it.
 *
 * @param block A block that passed `cock::exec::valid`.
 */
void __descriptors_import(const cock::abi::exec::ArgsBlock *block);

}

#endif // !LIBC_UNISTD_DESCRIPTOR_HPP
