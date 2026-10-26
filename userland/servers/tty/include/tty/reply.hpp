/** @file
 * @brief The answer words of a `tty` call.
 */
#ifndef TTY_REPLY_HPP
#define TTY_REPLY_HPP

#include <cock/abi/console.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::tty {

/** @brief Number of register words in a request or in an answer, header included. */
constexpr uint32_t ANSWER_WORDS =
	cock::abi::console::FIRST_DATA_WORD + cock::abi::console::DATA_WORDS;

/**
 * @brief Writes the header word of an answer.
 *
 * @param words The answer words.
 * @param status Result of the call.
 * @param count Number of bytes the answer carries.
 */
void answer(uint32_t (&words)[ANSWER_WORDS], cock::abi::console::Status status, size_t count);

} // namespace cock::tty

#endif // !TTY_REPLY_HPP
