/** @file
 * @brief Connect calls of programs `rs` has not registered yet, kept until it does.
 *
 * `rs` starts a program and registers it afterwards, so the first call of a new program can reach the name
 * server before the registration. Answering `UNKNOWN_CLIENT` would make the program retry and sometimes lose
 * the race. The call is detached instead, and answered when the registration arrives.
 */
#ifndef NAMES_PARKED_HPP
#define NAMES_PARKED_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::names {

/** @brief Most calls kept at once. */
constexpr size_t MAX_PARKED = 8;

/**
 * @brief Detaches the call being served and keeps its request.
 *
 * Call it while the call is served, before accepting another one. The caller stays blocked until
 * `answerParked` answers it, or until its own time limit runs out.
 *
 * @param process Process id of the caller.
 * @param path NUL terminated path of the server, already normalized.
 * @return true when the call was detached; false when the table is full of calls whose process is still
 *         alive, or no call is being served. Nothing is detached then and the caller answers as usual.
 */
bool park(uint32_t process, const char *path);

/**
 * @brief Answers the calls kept for a process, now that it is registered.
 *
 * Each call is served again as a normal `CONNECT` and its answer is delivered to the thread that waits.
 * A caller that timed out meanwhile or died does not receive it.
 *
 * @param process Process id that was just bound to a service.
 */
void answerParked(uint32_t process);

} // namespace cock::names

#endif // !NAMES_PARKED_HPP
