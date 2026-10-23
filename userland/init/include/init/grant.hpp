/** @file
 * @brief Giving capabilities to a started process and telling it so.
 */
#ifndef INIT_GRANT_HPP
#define INIT_GRANT_HPP

#include <cock/abi/manifest.hpp>
#include <stdint.h>

namespace cock::init {

/**
 * @brief Sends a `CAPABILITY_GRANTED` message to a process.
 *
 * `arg0` of the message carries the peer and `arg1` the argument.
 *
 * @param pid Process to tell.
 * @param peer Process id of its peer, or `NO_PROCESS` for none.
 * @param argument One number for the program, such as the block device of `fat`.
 */
void announce(uint32_t pid, uint32_t peer, uint32_t argument = 0);

/**
 * @brief Gives a process what its manifest entry says.
 *
 * Labels the process and grants its IPC capabilities (see `connectProgram`), grants every
 * capability of the entry, then announces the peer and the argument. Nothing is sent when the
 * entry has no grants, no peer and no argument. The kernel checks every grant against what
 * `init` holds.
 *
 * @param entry The manifest entry.
 * @param pid The process to equip.
 */
void equip(const cock::abi::manifest::Entry &entry, uint32_t pid);

} // namespace cock::init

#endif // !INIT_GRANT_HPP
