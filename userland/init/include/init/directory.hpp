/** @file
 * @brief What `init` tells the name server: which programs exist, who may talk to whom, and which
 * process runs each program.
 */
#ifndef INIT_DIRECTORY_HPP
#define INIT_DIRECTORY_HPP

#include <cock/abi/manifest.hpp>
#include <stdint.h>

namespace cock::init {

/**
 * @brief Brings the name server up.
 *
 * Finds the boot module `names` and checks that its process id is `NAMES_PID`, which every program
 * assumes. Gives it its service label and the right to pass on `IPC` capabilities, sends it
 * `CAPABILITY_GRANTED` and registers `ramfs`, which has no manifest entry. Prints `init: no name
 * server` when the module is missing or has another process id, and `init: cannot register <name>`
 * when the name server refuses.
 *
 * @return True when the name server is running and knows `ramfs`; false otherwise, and `init`
 * cannot start the system.
 */
bool startNames();

/**
 * @brief Registers every program of the manifest and the policy of its `ipc=` list.
 *
 * Sends the name server one `DEFINE` per entry (a program whose name is longer than the wire allows
 * is registered with no name and cannot be reached with `connect`) and one `ALLOW` per name in each
 * `ipc=` list. A name in a list that matches no entry prints `init: <program> cannot talk to <name>`.
 *
 * @return True when the name server accepted everything.
 */
bool describeSystem();

/**
 * @brief Tells the name server which process runs a program.
 *
 * Called after every start of a program, including a restart, so a client that connects after
 * that finds the new process. Prints `init: cannot bind process <pid> to label <label>` when the
 * name server refuses a process that still runs; a program that already ended is not an error.
 *
 * @param label Service label of the program.
 * @param pid Process id of the running program.
 * @return True when the name server accepted it.
 */
bool bindService(uint32_t label, uint32_t pid);

} // namespace cock::init

#endif // !INIT_DIRECTORY_HPP
