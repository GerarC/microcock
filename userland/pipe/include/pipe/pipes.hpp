/** @file
 * @brief The pipes: a ring of bytes with the number of read and write ends that are open.
 */
#ifndef PIPE_PIPES_HPP
#define PIPE_PIPES_HPP

#include <pipe/limits.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::pipe {

/** @brief One pipe. */
struct Pipe {
	bool used; /**< True when the slot holds a pipe. */
	uint8_t data[PIPE_BYTES]; /**< The ring of bytes. */
	size_t head; /**< Index of the oldest byte. */
	size_t count; /**< Bytes held. */
	uint32_t readers; /**< Read ends open. */
	uint32_t writers; /**< Write ends open. */
};

/** @brief The pipes, up to `MAX_PIPES`. */
extern Pipe pipes[MAX_PIPES];

/**
 * @brief Takes an unused pipe, empty and with no ends.
 * @return The pipe, or null when all `MAX_PIPES` are in use.
 */
Pipe *createPipe();

/**
 * @brief Puts bytes at the end of the pipe, as many as fit.
 * @param pipe The pipe.
 * @param bytes Source.
 * @param length Bytes offered.
 * @return Bytes taken, from zero (the pipe is full) to `length`.
 */
size_t pushBytes(Pipe &pipe, const uint8_t *bytes, size_t length);

/**
 * @brief Takes the oldest bytes out of the pipe.
 * @param pipe The pipe.
 * @param bytes Destination, at least `length` bytes.
 * @param length Most bytes wanted.
 * @return Bytes taken, from zero (the pipe is empty) to `length`.
 */
size_t popBytes(Pipe &pipe, uint8_t *bytes, size_t length);

/**
 * @brief Frees the pipe when no end of it is open, dropping what it holds.
 * @param pipe The pipe.
 */
void releaseIfUnused(Pipe &pipe);

} // namespace cock::pipe

#endif // !PIPE_PIPES_HPP
