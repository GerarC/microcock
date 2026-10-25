/** @file
 * @brief What `rs` tells the name server: which programs exist, who may talk to whom, and which
 * process runs each program.
 */
#ifndef RS_DIRECTORY_HPP
#define RS_DIRECTORY_HPP

#include <cock/abi/manifest.hpp>
#include <stdint.h>

namespace cock::rs {

/**
 * @brief Brings the name server up.
 *
 * Finds the boot module `names`, gives it its service label and the right to hand out handles on
 * every endpoint with the badge of its choice, binds it to its endpoint, makes the handle `rs`
 * calls it through (the one `SpawnRights::initialEndpoint` gives every program it starts is on the
 * same endpoint), sends it `CAPABILITY_GRANTED` and registers `ramfs`, which has no manifest entry.
 * Prints `rs: no name server` when the module is missing, and `rs: cannot register <name>` when
 * the name server refuses.
 *
 * @return True when the name server is running and knows `ramfs`; false otherwise, and `rs`
 * cannot start the system.
 */
bool startNames();

/**
 * @brief Registers every program of the manifest and the policy of its `ipc=` list.
 *
 * Sends the name server one `DEFINE` per entry (a program whose name is longer than the wire allows
 * is registered with no name and cannot be reached with `connect`) and one `ALLOW` per name in each
 * `ipc=` list. A name in a list that matches no entry prints `rs: <program> cannot talk to <name>`.
 *
 * @return True when the name server accepted everything.
 */
bool describeSystem();

/**
 * @brief Tells the name server which process runs a program.
 *
 * Called after every start of a program, including a restart, so a client that connects after
 * that finds the new process. It also binds the endpoint of the label to the process (see
 * `bindEndpoint`) and tells the name server which endpoint that is, so a client that connects
 * gets a handle on it. Prints `rs: cannot bind process <pid> to label <label>` when the
 * name server refuses a process that still runs; a program that already ended is not an error.
 *
 * @param label Service label of the program.
 * @param pid Process id of the running program.
 * @return True when the name server accepted it.
 */
bool bindService(uint32_t label, uint32_t pid);

/**
 * @brief Tells the name server that a process is a copy made by `fork` of another one.
 *
 * The copy then connects to what its parent may connect to and to nothing more (`Op::FORKED`). A request of the
 * copy that the name server kept waiting is answered. Prints `rs: cannot register copy <pid> of <pid>` when the
 * name server refuses a copy that still runs, for example because nothing known runs the parent.
 *
 * @param child Kernel process id of the copy.
 * @param parent Kernel process id of the process it copies.
 * @return True when the name server accepted it.
 */
bool registerFork(uint32_t child, uint32_t parent);

} // namespace cock::rs

#endif // !RS_DIRECTORY_HPP
