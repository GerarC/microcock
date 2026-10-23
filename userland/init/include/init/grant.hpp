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
 * `arg0` of the message carries the argument of the program.
 *
 * @param pid Process to tell.
 * @param argument One number for the program, such as the block device of `fat`.
 */
void announce(uint32_t pid, uint32_t argument = 0);

/**
 * @brief Gives a process what its manifest entry says.
 *
 * Grants every capability of the entry, then announces the argument. Nothing is sent when the
 * entry has no grants and no argument. The kernel checks every grant against what `init` holds.
 * The service label is not given here: `spawnProgram` passes it to the loader
 * (`cock::user::spawn`), so it exists before the program runs, and the right to talk to other
 * programs comes from the name server when the program connects.
 *
 * @param entry The manifest entry.
 * @param pid The process to equip.
 */
void equip(const cock::abi::manifest::Entry &entry, uint32_t pid);

} // namespace cock::init

#endif // !INIT_GRANT_HPP
