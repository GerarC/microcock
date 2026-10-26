/** @file
 * @brief Knowing which process is the `tty` server, the only one allowed to ask for an echo.
 */
#ifndef CONSOLE_TTY_LINK_HPP
#define CONSOLE_TTY_LINK_HPP

#include <stdint.h>

namespace cock::console {

/**
 * @brief Tells whether a caller is the `tty` server.
 *
 * The server is found by name (`cock::abi::console::TTY_NAME`) the first time and again whenever another process
 * calls, so a restarted `tty` is recognised.
 *
 * @param sender Process id of the caller, as the kernel gave it.
 * @return True when the name server says that `sender` is the `tty` server.
 */
bool isTty(uint32_t sender);

} // namespace cock::console

#endif // !CONSOLE_TTY_LINK_HPP
