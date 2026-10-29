/**
 * @file service.hpp
 * @brief Service labels: the stable names under which the name server applies its policy and the `IPC`
 * capability decides which labels a spawner may give.
 *
 * A label belongs to a program, not to a running process, so it survives a restart: `rs` gives the
 * same label to the new process that replaces a dead server, and the endpoint bound to that label
 * keeps working for its clients. The kernel compares no label when a message is sent: the handle is
 * the permission. A process has no label until `SYS_SERVICE_SET` gives it one, and the label
 * can be set once.
 */
#ifndef SHARED_COCK_ABI_SERVICE_HPP
#define SHARED_COCK_ABI_SERVICE_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::service {

/** Label of a process that has none. No capability covers it except the root one. */
constexpr uint32_t NO_SERVICE = 0;
/** Label that the kernel gives to the first program, `rs`. */
constexpr uint32_t ROOT_SERVICE = 1;
/** Label that `rs` gives to the `ramfs` boot module, which has no line in the manifest. */
constexpr uint32_t RAMFS_SERVICE = 2;
/** Label that `rs` gives to the name server, the second boot module. */
constexpr uint32_t NAMES_SERVICE = 3;
/** Process id of `rs`: the kernel starts it first, and the idle thread has id 0. */
constexpr uint32_t ROOT_PID = 1;
/** Label of the first program of the manifest; the program at index `n` gets `PROGRAM_SERVICE_BASE + n`. */
constexpr uint32_t PROGRAM_SERVICE_BASE = 16;

/**
 * @brief Label of the manifest entry at a position.
 * @param index Position of the entry in `manifest.conf`, from 0.
 * @return `PROGRAM_SERVICE_BASE + index`, the same for every process started from that entry and for the one that replaces it after a restart.
 */
constexpr uint32_t programLabel(size_t index) {
	return PROGRAM_SERVICE_BASE + static_cast<uint32_t>(index);
}
/** Number of labels that the root IPC capability covers, from `NO_SERVICE` on: all of them. */
constexpr uint64_t SERVICE_LABEL_COUNT = uint64_t(1) << 32;
/** Largest count of labels one `SYS_CAP_GRANT` can name, since its arguments have 32 bits: every label except the last. */
constexpr uint32_t GRANTABLE_LABEL_COUNT = UINT32_MAX;
/** Most IPC labels that one `SYS_PROC_CREATE` can give to the new process. */
constexpr size_t MAX_SPAWN_IPC = 16;
/** Bit of the mode of a service: the owner of the service may connect to it. */
constexpr uint32_t MODE_OWNER = 0x1;
/** Bit of the mode of a service: a program that is not the owner may connect to it. */
constexpr uint32_t MODE_OTHER = 0x2;
/** Mode of a service whose manifest line has no `mode=`: the owner and everybody else, as far as the `ipc=` lists allow. */
constexpr uint32_t MODE_ALL = MODE_OWNER | MODE_OTHER;

/**
 * @brief The rights a process is born with, given to `SYS_PROC_CREATE` as its fourth argument.
 *
 * The kernel checks them at creation and sets the label and derives one `IPC` capability for
 * each label of the list from a capability of the creator when `SYS_PROC_START` creates the
 * first thread, before it can run. Giving them
 * later with `SYS_SERVICE_SET` and `SYS_CAP_GRANT` leaves a window in which the program already
 * runs and is refused by its own servers.
 *
 * The same moment also gives the new process one handle, so that it can reach a server with no
 * grant message and no wait: the kernel derives an `ENDPOINT` capability from the creator, stamps
 * the process id of the new process as its badge and leaves the number of the handle in `EAX` when
 * the first thread starts (`cock::user::initialHandle()` returns it).
 */
struct SpawnRights {
	/** Service label of the new process, not `NO_SERVICE`. */
	uint32_t label;
	/** Number of valid entries of `ipc`, at most `MAX_SPAWN_IPC`. */
	uint32_t ipcCount;
	/** Labels the new process may talk to; the spawner must hold a grantable `IPC` capability for each. */
	uint32_t ipc[MAX_SPAWN_IPC];
	/** Endpoint the initial handle leads to, or `endpoint::NO_ENDPOINT` for none. The creator must hold a grantable `ENDPOINT` capability for it, with the mint right because the badge is chosen. */
	uint32_t initialEndpoint;
	/** Flags of the initial handle: `GRANT_DELEGATE` and `GRANT_MINT`, or zero for a plain handle. */
	uint32_t initialFlags;
};

} // namespace cock::abi::service

#endif // !SHARED_COCK_ABI_SERVICE_HPP
