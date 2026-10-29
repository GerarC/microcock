/** @file
 * @brief Asking the process manager to start one program of the manifest.
 */
#ifndef INIT_LAUNCH_HPP
#define INIT_LAUNCH_HPP

#include <cock/abi/manifest.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/pm.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::init {

/**
 * @brief Reads the program of an entry into an object `pm` made and asks `pm` to create the process.
 *
 * `init` holds no capability: it asks for the label of the entry and nothing else, and `pm` accepts it only when
 * `rs` permitted it. `pm` then tells `rs`, which registers the process with the name server. Prints
 * `init: started <name>` or the reason it could not start.
 *
 * @param files A connected file client.
 * @param process The client that reaches `pm`.
 * @param entry The manifest entry; its `path` names the file in the initrd and its `name` the process.
 * @param index Position of the entry in the manifest, which gives the label.
 * @return The kernel process id, or `NO_PROCESS` when the file cannot be read, is empty or too large, or `pm`
 * refused.
 */
uint32_t launch(cock::user::fs::Client &files, cock::user::pm::Client &process,
				const cock::abi::manifest::Entry &entry, size_t index);

} // namespace cock::init

#endif // !INIT_LAUNCH_HPP
