/** @file
 * @brief The process manager as `rs` sees it: starting it, the client that reaches it, and its exit notices.
 */
#ifndef RS_PM_HPP
#define RS_PM_HPP

#include <cock/abi/manifest.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/pm.hpp>
#include <stdint.h>

namespace cock::rs {

/** Name of the manifest entry that is the process manager. */
constexpr char PM_PROGRAM_NAME[] = "pm";

/**
 * @brief Starts `pm` and gives it what it needs, then tells it about the processes that already exist.
 *
 * `rs` creates `pm` itself, because nothing can create it for `rs`. It binds `pm` to its endpoint and to the name
 * server, grants it what the manifest line says (the right to create processes), the `IPC` capability for the
 * labels of the programs, an `ENDPOINT` capability on the endpoint of the name server (so the processes `pm`
 * creates can be given the initial handle) and a handle on the endpoint of `rs` that carries the badge of `pm`
 * (the exit notices come back through it), and sends `CAPABILITY_GRANTED` with that handle. Then it records in
 * the table of `pm` itself, the name server, the file server and `pm`.
 *
 * @param files A connected file client that reads the image of `pm`.
 * @param ramfs Kernel process id of the file server.
 * @return True when `pm` runs and answers; false when the manifest has no entry for it or it could not start.
 */
bool startPm(cock::user::fs::Client &files, uint32_t ramfs);

/**
 * @brief Finds the manifest entry of `pm`.
 * @return The entry, or null before `startPm` or when the manifest has none.
 */
const cock::abi::manifest::Entry *pmEntry();

/**
 * @brief The client that reaches `pm`.
 * @return The client; it works after `startPm` succeeded.
 */
cock::user::pm::Client &pmClient();

/**
 * @brief Records a process that already runs in the table of `pm`, with `rs` as its parent.
 * @param pid Kernel process id, such as a boot module.
 * @return True when `pm` recorded it.
 */
bool registerProcess(uint32_t pid);

/** Name of the manifest entry that is `init`, the first process of the system as Linux sees it. */
constexpr char INIT_PROGRAM_NAME[] = "init";

/**
 * @brief Tells whether a manifest entry is `init`.
 * @param candidate An entry of the manifest.
 * @return True when its name is `INIT_PROGRAM_NAME`.
 */
bool isInitEntry(const cock::abi::manifest::Entry &candidate);

/**
 * @brief The flags of a request to create the process of an entry.
 * @param candidate The entry.
 * @param notify True to have `pm` tell `rs` when the process ends.
 * @return `FORWARD_EXIT` when `notify` is true, plus `AS_INIT` for the entry of `init`.
 */
uint32_t flagsOf(const cock::abi::manifest::Entry &candidate, bool notify);

/**
 * @brief Finishes the start of a process that `init` asked `pm` for.
 *
 * Tells the name server which process runs the label and gives the process what its manifest entry says. A label
 * whose entry is not marked `parent=init` is ignored, so a forged notice changes nothing.
 *
 * @param pid Kernel process id of the new process.
 * @param label Service label it was created with.
 */
void startedForInit(uint32_t pid, uint32_t label);

/**
 * @brief Tells whether a message is the notice that `pm` created a process for `init`.
 * @param message A message that arrived in the inbox of `rs`.
 * @return True for a `PM_SPAWNED` message whose badge is the process id of `pm`. `arg0` is then the kernel
 * process id and `arg1` the service label.
 */
bool isSpawnNotice(const cock::ipc::Message &message);

/**
 * @brief Tells whether a message is the notice that `pm` made a copy of a process (`fork`).
 * @param message A message that arrived in the inbox of `rs`.
 * @return True for a `PM_FORKED` message whose badge is the process id of `pm`; the kernel stamps the badge, so no
 * other process can pass for it. `arg0` is then the kernel process id of the copy and `arg1` that of its parent.
 */
bool isForkNotice(const cock::ipc::Message &message);

/**
 * @brief Tells whether a message is an exit notice that `pm` sent on.
 * @param message A message that arrived in the inbox of `rs`.
 * @return True for a `PM_EXITED` message whose badge is the process id of `pm`; the kernel stamps the badge, so no
 * other process can pass for it. `arg0` is then the kernel process id and `arg1` the exit status.
 */
bool isExitNotice(const cock::ipc::Message &message);

} // namespace cock::rs

#endif // !RS_PM_HPP
