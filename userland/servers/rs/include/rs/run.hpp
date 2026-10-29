/** @file
 * @brief Running one manifest entry: configure a boot module or spawn a program.
 */
#ifndef RS_RUN_HPP
#define RS_RUN_HPP

#include <cock/abi/manifest.hpp>
#include <cock/user/fs.hpp>

namespace cock::rs {

/**
 * @brief Carries out one entry of the manifest.
 *
 * An entry without a path names a boot module: `rs` finds it and equips it, printing
 * `rs: configured <name>`. An entry with a path is spawned `repeat` times in a row and
 * equipped each time. When `repeat` is above one the runs are supervised: each is waited for
 * and the exit notices are counted and printed as `rs: <name> exit notices n/m`. A program
 * that fails to start ends the entry.
 *
 * @param client A connected file client.
 * @param entry The manifest entry.
 */
void runEntry(cock::user::fs::Client &client, const cock::abi::manifest::Entry &entry);

} // namespace cock::rs

#endif // !RS_RUN_HPP
