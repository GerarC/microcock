/** @file
 * @brief The names of the processes `pm` did not create: the root server tells `pm` the name of each one it registers.
 */
#ifndef PM_PROCESS_NAME_HPP
#define PM_PROCESS_NAME_HPP

#include <cock/abi/pm.hpp>
#include <pm/state.hpp>
#include <stdint.h>

namespace cock::pm {

/**
 * @brief Stores one chunk of the name of a process.
 *
 * The chunks come in order from 0. Chunk 0 empties the name before it stores its bytes, so a second `SET_NAME` replaces
 * the first. The last byte of the name is always zero, whatever the chunks hold.
 *
 * @param kernelId Kernel process id of a process that is in the table.
 * @param chunk Number of the chunk, from 0 to `abi::pm::NAME_CHUNKS` - 1.
 * @param first The first `sizeof(uint32_t)` bytes of the chunk.
 * @param second The next `sizeof(uint32_t)` bytes of the chunk.
 * @return `OK`; `NOT_FOUND` when the process is not in the table; `BAD_REQUEST` for a chunk past the last.
 */
cock::abi::pm::Status setProcessName(uint32_t kernelId, uint32_t chunk, uint32_t first, uint32_t second);

} // namespace cock::pm

#endif // !PM_PROCESS_NAME_HPP
