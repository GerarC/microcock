/** @file
 * @brief Readers that wait for a finished line, answered later with a deferred reply.
 */
#ifndef TTY_READERS_HPP
#define TTY_READERS_HPP

#include <stddef.h>
#include <stdint.h>
#include <tty/reply.hpp>
#include <tty/terminal.hpp>

namespace cock::tty {

/**
 * @brief Tells whether a read on a terminal could be answered now.
 *
 * @param terminal The terminal.
 * @return True when finished input waits, or bytes taken for a reader that died wait to be given again.
 */
bool inputReady(const Terminal &terminal);

/**
 * @brief Fills an answer with bytes taken from the finished lines of a terminal.
 *
 * The bytes that a dead reader did not receive (see `wakeReaders`) come first.
 *
 * @param terminal The terminal.
 * @param reply The answer words, header included.
 * @param wanted Most bytes the reader asked for.
 */
void fillRead(Terminal &terminal, uint32_t (&reply)[ANSWER_WORDS], size_t wanted);

/**
 * @brief Puts the caller of the current call on hold until a line is ready on a terminal.
 *
 * The call is deferred with `SYS_IPC_DEFER`, so its answer is sent later by `wakeReaders`.
 *
 * @param terminal The terminal.
 * @param length Most bytes the reader asked for.
 * @return True when the call was deferred, false when the table of readers of the terminal is full or the
 * kernel has no ticket; the caller then answers `BUSY`.
 */
bool deferRead(Terminal &terminal, size_t length);

/**
 * @brief Answers the held readers of every terminal, oldest first, while there are finished lines.
 *
 * When the reply cannot be delivered because the reader died, the bytes are kept and the next reader of that terminal
 * gets them first, so a finished line is not lost.
 */
void wakeReaders();

} // namespace cock::tty

#endif // !TTY_READERS_HPP
