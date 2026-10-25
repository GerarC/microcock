/** @file
 * @brief Giving capabilities to a started process and telling it so.
 */
#ifndef RS_GRANT_HPP
#define RS_GRANT_HPP

#include <cock/abi/manifest.hpp>
#include <stdint.h>

namespace cock::rs {

/**
 * @brief Sends a `CAPABILITY_GRANTED` message to a process.
 *
 * `arg0` of the message carries the argument of the program and `arg1` a second number, which only the start of `pm` uses.
 *
 * Goes to the endpoint of the label (`sendTo`), so the process must already be bound to it.
 *
 * @param label Service label of the process to tell.
 * @param argument One number for the program, such as the block device of `fat`.
 * @param second A second number, zero for every program but `pm`.
 */
void announce(uint32_t label, uint32_t argument = 0, uint32_t second = 0);

/**
 * @brief Grants a process every capability of its manifest entry, and announces nothing.
 *
 * The kernel checks every grant against what `rs` holds. `equip` uses it, and so does the start of `pm`,
 * which sends its own announcement.
 *
 * @param entry The manifest entry.
 * @param pid The process that receives the capabilities.
 */
void grantCapabilities(const cock::abi::manifest::Entry &entry, uint32_t pid);

/**
 * @brief Gives a process what its manifest entry says.
 *
 * Grants every capability of the entry, then announces the argument. Nothing is sent when the
 * entry has no grants and no argument. The kernel checks every grant against what `rs` holds.
 * The service label is not given here: `spawnProgram` passes it to the loader
 * (`cock::user::spawn`), so it exists before the program runs, and the right to talk to other
 * programs comes from the name server when the program connects.
 *
 * @param entry The manifest entry.
 * @param pid The process to equip.
 */
void equip(const cock::abi::manifest::Entry &entry, uint32_t pid);

} // namespace cock::rs

#endif // !RS_GRANT_HPP
