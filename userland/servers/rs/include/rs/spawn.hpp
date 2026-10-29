/** @file
 * @brief Starting a program from an image in the initrd.
 */
#ifndef RS_SPAWN_HPP
#define RS_SPAWN_HPP

#include <cock/abi/manifest.hpp>
#include <cock/user/fs.hpp>
#include <stdint.h>

namespace cock::rs {

/**
 * @brief Reads the program of an entry straight into an object the process manager made, asks it to create the
 * process and tells the name server which process runs it.
 *
 * `pm` becomes the kernel parent of the process. Prints `rs: started <name>` or the reason it could not start.
 * The image must not be empty and must fit in `abi::pm::MAX_IMAGE_BYTES`.
 *
 * @param client A connected file client.
 * @param entry The manifest entry; its `path` names the file and its `name` the process.
 * @param notify True to have `pm` send a `PM_EXITED` message when the process ends.
 * @return The kernel process id, or `NO_PROCESS` on failure.
 */
uint32_t spawnProgram(cock::user::fs::Client &client, const cock::abi::manifest::Entry &entry,
					  bool notify);

/**
 * @brief Reads the program of an entry into an anonymous mapping and creates the process itself, with no
 * exit notice.
 *
 * Used only for `pm`, which cannot ask itself to be created. The caller binds and equips the process.
 * Prints `rs: started <name>` or the reason it could not start.
 *
 * @param client A connected file client.
 * @param entry The manifest entry; its `path` names the file and its `name` the process.
 * @return The kernel process id, or `NO_PROCESS` on failure.
 */
uint32_t spawnDirect(cock::user::fs::Client &client, const cock::abi::manifest::Entry &entry);

} // namespace cock::rs

#endif // !RS_SPAWN_HPP
