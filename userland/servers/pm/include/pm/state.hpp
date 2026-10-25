/** @file
 * @brief What `pm` keeps between requests: its table of processes, the handle on the root server, and what it
 * lets `init` do.
 */
#ifndef PM_STATE_HPP
#define PM_STATE_HPP

#include <cock/abi/pm.hpp>
#include <cock/pm/table.hpp>
#include <stdint.h>

namespace cock::pm {

/** The table of processes. Only the main thread of `pm` touches it. */
extern Table table;

/** Handle on the endpoint of the root server, set once the grant arrived; the notices go through it. */
extern uint32_t rootHandle;

/** Id of the endpoint of the name server, which every process `init` asks for gets as its initial handle. */
extern uint32_t namesEndpoint;

/** Who is calling, from the badge of its handle. */
enum class Caller : uint32_t {
	/** The root server, which may ask for everything. */
	ROOT,
	/** The process that runs as `init`, which may start the programs the root server permitted. */
	INIT,
	/** Anyone else, who may only read the table. */
	OTHER,
};

/**
 * @brief Classifies a caller.
 * @param kernelId Badge of the handle: the kernel process id of the caller.
 * @return `ROOT` for the root server; `INIT` when the row of that process has `INIT_PID` and runs; `OTHER` otherwise.
 */
Caller callerOf(uint32_t kernelId);

/**
 * @brief Lets `init` create processes with a service label.
 * @param label A label of a program, from `PROGRAM_SERVICE_BASE` on, within `PROGRAM_LABEL_COUNT`.
 * @return True when it is in the list now; false when the label is out of range or the list is full.
 */
bool permit(uint32_t label);

/**
 * @brief Tells whether `init` may create processes with a label.
 * @param label A service label.
 * @return True when `permit` accepted it.
 */
bool permitted(uint32_t label);

} // namespace cock::pm

#endif // !PM_STATE_HPP
