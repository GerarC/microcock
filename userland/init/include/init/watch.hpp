/** @file
 * @brief Servers that `init` starts again when they end.
 */
#ifndef INIT_WATCH_HPP
#define INIT_WATCH_HPP

#include <cock/abi/manifest.hpp>
#include <cock/user/fs.hpp>
#include <stdint.h>

namespace cock::init {

/** Length of one restart window in milliseconds of waiting by `init`. */
constexpr uint32_t RESTART_WINDOW_MILLISECONDS = 60000;
/** Longest wait for a message in the supervision loop, in milliseconds; it is also how the loop counts time. */
constexpr uint32_t WATCH_POLL_MILLISECONDS = 100;

/**
 * @brief Puts a started program under supervision.
 *
 * The program must have been started with the exit notice requested. Does nothing for an entry
 * whose `restarts` is `NO_RESTARTS`.
 *
 * @param entry Manifest entry the program was started from; it must stay valid for the life of `init`.
 * @param pid Process id of the running program.
 * @return True when the program is now watched, false when the entry asks for no restart or the
 * table is full.
 */
bool watchProgram(const cock::abi::manifest::Entry &entry, uint32_t pid);

/**
 * @brief Tells whether any program is still watched.
 * @return True while at least one program can be started again.
 */
bool anyWatched();

/**
 * @brief Starts watched programs again as they end, until none can be restarted.
 *
 * Reads the exit notices that the kernel sends and the ones kept by `awaitExit`. When a watched
 * program ends it is spawned again from the same entry and given its capabilities and peer, and
 * `init: restarted <name> <n> of <limit>` is printed. Each program may be restarted `restarts`
 * times in one window of `RESTART_WINDOW_MILLISECONDS`; the counter starts again when the window
 * is over. Past the limit, or when the program cannot be spawned, `init: <name> gave up` is
 * printed and the program is no longer watched. Time is counted only while the loop waits with
 * nothing to do, so a program that crashes in a tight loop reaches its limit sooner, never later.
 *
 * @param client File connection that reads the program images.
 * @return When no program is watched any more, or when the receive fails.
 */
void superviseServers(cock::user::fs::Client &client);

} // namespace cock::init

#endif // !INIT_WATCH_HPP
