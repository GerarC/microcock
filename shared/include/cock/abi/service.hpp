/**
 * @file service.hpp
 * @brief Service labels: the stable names under which the IPC capability authorizes communication.
 *
 * A label belongs to a program, not to a running process, so it survives a restart: `init` gives the
 * same label to the new process that replaces a dead server, and every capability held for that
 * label keeps working. A process has no label until `SYS_SERVICE_SET` gives it one, and the label
 * can be set once.
 */
#ifndef SHARED_COCK_ABI_SERVICE_HPP
#define SHARED_COCK_ABI_SERVICE_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::service {

/** Label of a process that has none. No capability covers it except the root one. */
constexpr uint32_t NO_SERVICE = 0;
/** Label that the kernel gives to the first program, `init`. */
constexpr uint32_t ROOT_SERVICE = 1;
/** Label that `init` gives to the `ramfs` boot module, which has no line in the manifest. */
constexpr uint32_t RAMFS_SERVICE = 2;
/** Label of the first program of the manifest; the program at index `n` gets `PROGRAM_SERVICE_BASE + n`. */
constexpr uint32_t PROGRAM_SERVICE_BASE = 16;
/** Number of labels that the root IPC capability covers, from `NO_SERVICE` on: all of them. */
constexpr uint64_t SERVICE_LABEL_COUNT = uint64_t(1) << 32;
/** Most IPC labels that one `SYS_PROC_CREATE` can give to the new process. */
constexpr size_t MAX_SPAWN_IPC = 8;

/**
 * @brief The rights a process is born with, given to `SYS_PROC_CREATE` as its fourth argument.
 *
 * The kernel checks them at creation and sets the label and derives one `IPC` capability for
 * each label of the list from a capability of the creator when `SYS_PROC_START` creates the
 * first thread, before it can run. Giving them
 * later with `SYS_SERVICE_SET` and `SYS_CAP_GRANT` leaves a window in which the program already
 * runs and is refused by its own servers.
 */
struct SpawnRights {
	/** Service label of the new process, not `NO_SERVICE`. */
	uint32_t label;
	/** Number of valid entries of `ipc`, at most `MAX_SPAWN_IPC`. */
	uint32_t ipcCount;
	/** Labels the new process may talk to; the spawner must hold a grantable `IPC` capability for each. */
	uint32_t ipc[MAX_SPAWN_IPC];
};

} // namespace cock::abi::service

#endif // !SHARED_COCK_ABI_SERVICE_HPP
