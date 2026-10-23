/** @file
 * @brief The text of each writer, kept until its last chunk so concurrent writes do not mix.
 */
#ifndef CONSOLE_WRITERS_HPP
#define CONSOLE_WRITERS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::console {

/**
 * @brief Adds a chunk of text from a writer, and prints it all when the chunk is the last.
 *
 * A writer that sends more than its slot holds has the text printed early. When every slot is
 * taken by live writers the chunk is printed at once, unbuffered. Output goes to the serial
 * line and to the screen.
 *
 * @param sender Process id of the writer.
 * @param bytes The chunk.
 * @param length Number of bytes in the chunk.
 * @param last True when this chunk ends the write.
 */
void handleWrite(uint32_t sender, const char *bytes, size_t length, bool last);

/**
 * @brief Tells whether some writer has text waiting for its last chunk.
 *
 * @return True when any slot holds text.
 */
bool hasPartialText();

/**
 * @brief Prints the text of writers that died before their last chunk, and frees their slots.
 */
void sweepDeadWriters();

} // namespace cock::console

#endif // !CONSOLE_WRITERS_HPP
