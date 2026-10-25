/** @file
 * @brief Readers that wait for a finished line, answered later with a deferred reply.
 */
#ifndef CONSOLE_READERS_HPP
#define CONSOLE_READERS_HPP

#include <console/reply.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::console {

/**
 * @brief Fills an answer with bytes taken from the finished lines.
 *
 * @param reply The answer words, header included.
 * @param wanted Most bytes the reader asked for.
 */
void fillRead(uint32_t (&reply)[ANSWER_WORDS], size_t wanted);

/**
 * @brief Puts the caller of the current call on hold until a line is ready.
 *
 * The call is deferred with `SYS_IPC_DEFER`, so its answer is sent later by `wakeReaders`.
 *
 * @param length Most bytes the reader asked for.
 * @return True when the call was deferred, false when the table of readers is full or the
 * kernel has no ticket; the caller then answers `BUSY`.
 */
bool deferRead(size_t length);

/**
 * @brief Answers the held readers, oldest first, while there are finished lines.
 */
void wakeReaders();

} // namespace cock::console

#endif // !CONSOLE_READERS_HPP
