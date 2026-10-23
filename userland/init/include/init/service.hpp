/** @file
 * @brief Service labels and the IPC capabilities that `init` gives to each program.
 */
#ifndef INIT_SERVICE_HPP
#define INIT_SERVICE_HPP

#include <cock/abi/manifest.hpp>
#include <cock/abi/service.hpp>
#include <stdint.h>

namespace cock::init {

/**
 * @brief Service label of a manifest entry.
 *
 * The label is `PROGRAM_SERVICE_BASE` plus the position of the entry in the manifest, so it is
 * the same for every process started from that entry, and for the process that replaces it after
 * a restart.
 *
 * @param entry An entry of `cock::init::manifest`.
 * @return The label.
 */
uint32_t labelOf(const cock::abi::manifest::Entry &entry);

/**
 * @brief Service label of a program named in an `ipc` list.
 * @param name Name of a manifest entry, or the name of the `ramfs` boot module, which has no entry.
 * @return The label, or `NO_SERVICE` when no entry has that name.
 */
uint32_t labelOfName(const char *name);

/**
 * @brief The rights a program is born with: its label and the labels of its `ipc` list.
 *
 * Given to `SYS_SPAWN`, which sets them before the first thread of the program can run. A name
 * that matches no entry prints `init: <program> cannot talk to <name>` and is left out.
 *
 * @param entry The manifest entry the program is started from.
 * @return The rights to pass to `cock::user::spawn`.
 */
cock::abi::service::SpawnRights rightsOf(const cock::abi::manifest::Entry &entry);

/**
 * @brief Labels a boot module and gives it the IPC capabilities of its entry.
 *
 * Boot modules are started by the kernel, so this happens after they run: it is meant for
 * programs that only wait for messages, such as the display server. A refused label or grant
 * prints `init: cannot label <program>` or `init: <program> cannot talk to <name>`.
 *
 * @param entry The manifest entry of the boot module.
 * @param pid Process id of the boot module.
 */
void connectBootModule(const cock::abi::manifest::Entry &entry, uint32_t pid);

} // namespace cock::init

#endif // !INIT_SERVICE_HPP
