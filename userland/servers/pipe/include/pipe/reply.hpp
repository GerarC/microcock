/** @file
 * @brief The answer words of a pipe call.
 */
#ifndef PIPE_REPLY_HPP
#define PIPE_REPLY_HPP

#include <cock/abi/pipe.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::pipe {

/** @brief Number of register words in a request or in an answer, header included. */
constexpr size_t CALL_WORDS = cock::abi::pipe::CALL_WORDS;

/**
 * @brief Writes the header word of an answer.
 *
 * @param words The answer words.
 * @param status Result of the call.
 * @param count Number of bytes the answer carries, or the bytes taken.
 */
void answer(uint32_t (&words)[CALL_WORDS], cock::abi::pipe::Status status, size_t count);

} // namespace cock::pipe

#endif // !PIPE_REPLY_HPP
