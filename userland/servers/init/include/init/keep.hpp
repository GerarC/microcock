/** @file
 * @brief Starting the programs of `init` and starting them again when they end.
 */
#ifndef INIT_KEEP_HPP
#define INIT_KEEP_HPP

#include <cock/user/fs.hpp>
#include <cock/user/pm.hpp>
#include <stdint.h>

namespace cock::init {

/** Milliseconds between two looks at the programs `init` keeps. */
constexpr uint32_t KEEP_POLL_MILLISECONDS = 250;
/** Times `init` starts one program again before it gives up on it. */
constexpr uint32_t MAX_RESTARTS = 8;

/**
 * @brief Starts every manifest entry marked `parent=init`, in file order, and keeps them running.
 *
 * Looks at each program every `KEEP_POLL_MILLISECONDS` with `process_alive` and starts it again when it ended
 * or never started, at most `MAX_RESTARTS` times after the first start; then it prints `init: <name> gave up` and leaves it. Polling
 * stands in for `wait`, which `pm` does not offer yet.
 *
 * @param files A connected file client.
 * @param process The client that reaches `pm`.
 * @return Never, while at least one program can still be restarted; it returns when every one gave up or there
 * was none.
 */
void keepRunning(cock::user::fs::Client &files, cock::user::pm::Client &process);

} // namespace cock::init

#endif // !INIT_KEEP_HPP
