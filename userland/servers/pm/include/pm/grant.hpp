/** @file
 * @brief Waiting for what `rs` gives the process manager before it serves anything.
 */
#ifndef PM_GRANT_HPP
#define PM_GRANT_HPP

#include <stdint.h>

namespace cock::pm {

/** What the grant message carries. */
struct Grant {
	/** Handle `pm` holds on the endpoint of the root server; the notices go through it. */
	uint32_t rootHandle;
	/** Id of the endpoint of the name server, for the initial handle of the processes `init` asks for. */
	uint32_t namesEndpoint;
};

/**
 * @brief Blocks until the `CAPABILITY_GRANTED` message of the root server arrives.
 *
 * `rs` sends it after it gave `pm` the right to create processes, the labels of the programs, the endpoint
 * of the name server and a handle on its own endpoint. The badge of the message must be `ROOT_PID`; any other
 * asynchronous message is dropped. Synchronous calls that arrive earlier wait in the queue of the kernel and
 * are served afterwards.
 *
 * @return `arg0` of the message as the handle on the root server and `arg1` as the endpoint of the name server.
 */
Grant waitForCapabilities();

} // namespace cock::pm

#endif // !PM_GRANT_HPP
