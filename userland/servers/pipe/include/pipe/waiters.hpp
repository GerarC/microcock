/** @file
 * @brief Callers held while a pipe is empty or full, answered later with a deferred reply.
 */
#ifndef PIPE_WAITERS_HPP
#define PIPE_WAITERS_HPP

#include <pipe/reply.hpp>
#include <stdint.h>

namespace cock::pipe {

/**
 * @brief Puts the caller of the current call on hold.
 *
 * The call is detached with `SYS_IPC_DEFER`, and the request is kept so `retryWaiters` can try it again.
 *
 * @param sender Process id of the caller.
 * @param request The request words.
 * @return true when the call was held; false when the table of waiting callers is full or the kernel has no
 * ticket, and the caller must be answered `BUSY`.
 */
bool holdCall(uint32_t sender, const uint32_t (&request)[CALL_WORDS]);

/**
 * @brief Tries the held calls again, in table order, and answers each one that can be served.
 *
 * Repeats until a whole pass answers nobody, because an answer can free room or data for another waiter.
 */
void retryWaiters();

/**
 * @brief Closes the ends of dead processes, forgets their held calls and tries the rest again.
 *
 * Run when the server has waited `SWEEP_MS` with no call, so a reader whose writer died without closing sees the
 * end of the file.
 */
void sweepDead();

} // namespace cock::pipe

#endif // !PIPE_WAITERS_HPP
