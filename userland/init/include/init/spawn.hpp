/** @file
 * @brief Starting a program from an image in the initrd.
 */
#ifndef INIT_SPAWN_HPP
#define INIT_SPAWN_HPP

#include <cock/abi/manifest.hpp>
#include <cock/user/fs.hpp>
#include <stdint.h>

namespace cock::init {

/**
 * @brief Reads the program of an entry into an anonymous mapping and spawns it.
 *
 * Prints `init: started <name>` or the reason it could not start. The image must not be empty
 * and must be at most 1 MiB.
 *
 * @param client A connected file client.
 * @param entry The manifest entry; its `path` names the file and its `name` the process.
 * @param notify True to ask the kernel for a `PROCESS_EXITED` message when it ends.
 * @return The process id, or `NO_PROCESS` on failure.
 */
uint32_t spawnProgram(cock::user::fs::Client &client, const cock::abi::manifest::Entry &entry,
					  bool notify);

} // namespace cock::init

#endif // !INIT_SPAWN_HPP
