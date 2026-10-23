/** @file
 * @brief Waiting for a started process to end.
 */
#ifndef INIT_SUPERVISE_HPP
#define INIT_SUPERVISE_HPP

#include <cock/ipc/message.hpp>
#include <stdint.h>

namespace cock::init {

/**
 * @brief Keeps an exit notice that arrived while `init` waited for something else.
 *
 * Fits the `onForeign` hook of the file client. Puts a `PROCESS_EXITED` message from the kernel
 * in the stash of the C++ API, where `superviseServers` reads it; any other message is dropped.
 * The stash holds `STASH_CAPACITY` messages; a notice that finds it full is lost.
 *
 * @param message The message that was not the awaited reply.
 */
void keepExitNotice(const cock::ipc::Message &message);

/**
 * @brief Waits until a process has ended.
 *
 * Receives with a timeout of 20 ms and, between messages, asks the kernel whether the process
 * is alive, so an exit is noticed even if its notice is lost. A notice already kept by
 * `keepExitNotice` for this process counts. The exit notice of any other process is kept in the
 * stash of the C++ API, so `superviseServers` still reads it.
 *
 * @param pid The process, started with the exit notice requested.
 * @return True when the `PROCESS_EXITED` notice from the kernel arrived, false when the
 * process was found dead without one or the receive failed.
 */
bool awaitExit(uint32_t pid);

} // namespace cock::init

#endif // !INIT_SUPERVISE_HPP
