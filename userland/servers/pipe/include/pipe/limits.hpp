/** @file
 * @brief Sizes of the tables of the pipe server.
 */
#ifndef PIPE_LIMITS_HPP
#define PIPE_LIMITS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::pipe {

/** @brief Most pipes that exist at once. */
constexpr size_t MAX_PIPES = 16;
/** @brief Bytes a pipe holds before a writer has to wait. */
constexpr size_t PIPE_BYTES = 512;
/** @brief Most ends, read and write together, that all processes hold at once. */
constexpr size_t MAX_ENDS = 64;
/** @brief Most callers held while a pipe is empty or full. */
constexpr size_t MAX_WAITERS = 16;
/** @brief Longest wait of the server, in milliseconds, while any end is open: after it the ends of dead processes are closed. */
constexpr uint32_t SWEEP_MS = 50;

} // namespace cock::pipe

#endif // !PIPE_LIMITS_HPP
