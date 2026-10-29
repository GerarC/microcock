/** @file
 * @brief Reading a file or the standard input chunk by chunk, for the programs of `/bin` that filter bytes.
 *
 * Everything is inline and nothing is allocated: the chunk lives on the stack of the caller and the consumer is a
 * template parameter, so a program pays only for the loop it writes. A program that includes this file lists the
 * file client of the C library (`STREAM_CLIENT = yes`) and `tool.cpp` in its `Makefile`.
 */
#ifndef COCK_USER_STREAM_HPP
#define COCK_USER_STREAM_HPP

#include <cock/user/tool.hpp>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

namespace cock::user::tool {

/** @brief Descriptor of the standard input. */
constexpr int STDIN_DESCRIPTOR = 0;

/** @brief Descriptor of the standard output. */
constexpr int STDOUT_DESCRIPTOR = 1;

/** @brief Bytes read at a time. */
constexpr size_t CHUNK_BYTES = 512;

/** @brief Result of `read` at the end of the input. */
constexpr ssize_t END_OF_INPUT = 0;

/** @brief Subject printed when the standard input fails. */
constexpr char STANDARD_INPUT_NAME[] = "standard input";

/** @brief What a consumer tells the reading loop after each chunk. */
enum class Flow {
	/** Send the next chunk. */
	MORE,
	/** Stop reading: the consumer has what it needs. Not an error. */
	DONE,
	/** Stop reading: the consumer failed and has already said so. The read ends with `STATUS_FAILURE`. */
	ERROR,
};

/**
 * @brief Writes every byte of a buffer to a descriptor, looping over short writes.
 *
 * @param descriptor An open descriptor.
 * @param bytes First byte.
 * @param count Number of bytes.
 * @return True when all of them were written; false at the first write that fails or makes no progress (`errno` has
 * the reason).
 */
inline bool writeAll(int descriptor, const char *bytes, size_t count) {
	size_t done = 0;
	while (done < count) {
		ssize_t written = ::write(descriptor, bytes + done, count - done);
		if (written <= END_OF_INPUT) return false;
		done += static_cast<size_t>(written);
	}
	return true;
}

/**
 * @brief Reads a descriptor until it ends, handing each chunk to a consumer.
 *
 * @tparam Consumer Callable taking `(const char *bytes, size_t count)` and returning a `Flow`.
 * @param program Name of the program, for the message of a failed read.
 * @param subject What is being read, for the same message.
 * @param descriptor An open descriptor.
 * @param consumer Called once per chunk, in order.
 * @return `STATUS_OK` at the end of the input or when the consumer said `DONE`; `STATUS_FAILURE` after printing
 * `<program>: <subject>: <reason>` when `read` fails, or silently when the consumer said `ERROR`.
 */
template <typename Consumer>
int readDescriptor(const char *program, const char *subject, int descriptor, Consumer &&consumer) {
	char chunk[CHUNK_BYTES] = {};
	for (;;) {
		ssize_t count = ::read(descriptor, chunk, sizeof(chunk));
		if (count == END_OF_INPUT) return STATUS_OK;
		if (count < END_OF_INPUT) return fail(program, subject);

		Flow flow = consumer(static_cast<const char *>(chunk), static_cast<size_t>(count));
		if (flow == Flow::DONE) return STATUS_OK;
		if (flow == Flow::ERROR) return STATUS_FAILURE;
	}
}

/**
 * @brief Opens a path for reading, reads it to its end and closes it; with no path, reads the standard input.
 *
 * @tparam Consumer Callable as in `readDescriptor`.
 * @param program Name of the program, for messages.
 * @param path Path as typed (the C library resolves it against the working directory), or null for the standard input.
 * @param consumer Called once per chunk.
 * @return As `readDescriptor`; `STATUS_FAILURE` after printing `<program>: <path>: <reason>` when it cannot be opened.
 */
template <typename Consumer>
int readPath(const char *program, const char *path, Consumer &&consumer) {
	if (path == nullptr) return readDescriptor(program, STANDARD_INPUT_NAME, STDIN_DESCRIPTOR, consumer);

	int descriptor = ::open(path, O_RDONLY);
	if (descriptor < 0) return fail(program, path);

	int status = readDescriptor(program, path, descriptor, consumer);
	::close(descriptor);
	return status;
}

} // namespace cock::user::tool

#endif // !COCK_USER_STREAM_HPP
