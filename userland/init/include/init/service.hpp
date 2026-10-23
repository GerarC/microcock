/** @file
 * @brief Service labels and the IPC capabilities that `init` gives to each program.
 */
#ifndef INIT_SERVICE_HPP
#define INIT_SERVICE_HPP

#include <cock/abi/manifest.hpp>
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
 * @brief Labels a started process and gives it the IPC capabilities of its entry.
 *
 * Sets the label of the entry on the process, then grants one `IPC` capability for the label of
 * each name in the `ipc` list. A name that matches no entry, or a refused grant, prints
 * `init: <program> cannot talk to <name>`; a refused label prints `init: cannot label <program>`.
 * Nothing is printed for a process that has already ended, which is normal for programs that
 * exit at once.
 * The label is known from the manifest, so the order in which programs start does not matter and
 * a restarted server needs no new grant for its clients.
 *
 * @param entry The manifest entry the process was started from.
 * @param pid Process id of the new process.
 */
void connectProgram(const cock::abi::manifest::Entry &entry, uint32_t pid);

} // namespace cock::init

#endif // !INIT_SERVICE_HPP
