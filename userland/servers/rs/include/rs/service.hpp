/** @file
 * @brief Service labels, and the label each program is born with or given.
 */
#ifndef RS_SERVICE_HPP
#define RS_SERVICE_HPP

#include <cock/abi/manifest.hpp>
#include <cock/abi/service.hpp>
#include <stdint.h>

namespace cock::rs {

/**
 * @brief Service label of a manifest entry.
 *
 * The label is `PROGRAM_SERVICE_BASE` plus the position of the entry in the manifest, so it is
 * the same for every process started from that entry, and for the process that replaces it after
 * a restart.
 *
 * @param entry An entry of `cock::rs::manifest`.
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
 * @brief The rights a program is born with: its label, no `IPC` capability and one handle on the
 * endpoint of the name server (`initialEndpoint`, which makes the endpoint the first time).
 *
 * Given to the loader (`cock::user::spawn`), which has the kernel set them before the first thread
 * of the program can run, with the number of the handle in `EAX`. Handles on other programs come
 * later, when the program connects through the name server.
 *
 * @param entry The manifest entry the program is started from.
 * @return The rights to pass to `cock::user::spawn`.
 */
cock::abi::service::SpawnRights rightsOf(const cock::abi::manifest::Entry &entry);

/**
 * @brief Labels a boot module and tells the name server which process runs it.
 *
 * Boot modules are started by the kernel, so this happens after they run: it is meant for
 * programs that only wait for messages, such as the display server. A refused label prints
 * `rs: cannot label <program>`.
 *
 * @param entry The manifest entry of the boot module.
 * @param pid Process id of the boot module.
 * @return True when the label was set and the name server accepted the binding.
 */
bool labelBootModule(const cock::abi::manifest::Entry &entry, uint32_t pid);

} // namespace cock::rs

#endif // !RS_SERVICE_HPP
