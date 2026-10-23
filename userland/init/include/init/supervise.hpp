/** @file
 * @brief Waiting for a started process to end.
 */
#ifndef INIT_SUPERVISE_HPP
#define INIT_SUPERVISE_HPP

#include <stdint.h>

namespace cock::init {

/**
 * @brief Waits until a process has ended.
 *
 * Receives with a timeout of 20 ms and, between messages, asks the kernel whether the process
 * is alive, so an exit is noticed even if its notice is lost.
 *
 * @param pid The process, started with the exit notice requested.
 * @return True when the `PROCESS_EXITED` notice from the kernel arrived, false when the
 * process was found dead without one or the receive failed.
 */
bool awaitExit(uint32_t pid);

} // namespace cock::init

#endif // !INIT_SUPERVISE_HPP
