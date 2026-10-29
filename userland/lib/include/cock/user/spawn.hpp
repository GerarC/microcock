/**
 * @file spawn.hpp
 * @brief The loader of programs in user space: reads an ELF image and builds a process with the
 * `SYS_PROC_*` calls.
 *
 * The kernel only offers primitives (create an empty process, map pages, copy bytes, make pages
 * read only, start). Everything that knows the ELF format is here, so a mistake in this file can
 * hurt only the program that runs it: the kernel checks every range again.
 */
#ifndef COCK_USER_SPAWN_HPP
#define COCK_USER_SPAWN_HPP

#include <cock/abi/executable.hpp>
#include <cock/abi/process.hpp>
#include <cock/abi/service.hpp>
#include <cock/elf/image.hpp>
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::user {

/**
 * @brief Maps and fills the segments of a parsed image into a pending process.
 * @param handle Handle from `proc_create`.
 * @param data The ELF image the segments come from.
 * @param image The result of `elf::parse` for `data`.
 * @return true when every segment is mapped, filled and, when it is not writable, sealed.
 *         false on the first call that fails; the process is left half built.
 */
inline bool load_segments(uint32_t handle, const uint8_t *data, const elf::Image &image) {
	for (size_t index = 0; index < image.segmentCount; index++) {
		const elf::Segment &segment = image.segments[index];
		if (!proc_map(handle, elf::firstPage(segment), elf::pageCount(segment))) return false;
		if (segment.fileSize != 0 &&
			!proc_write(handle, segment.address, data + segment.offset, segment.fileSize))
			return false;
		if (!segment.writable &&
			!proc_protect(handle, elf::firstPage(segment), elf::pageCount(segment)))
			return false;
	}
	return true;
}

/**
 * @brief Starts a process from an ELF image in memory.
 *
 * Only a process that holds a `SPAWN` capability may call it. The new process starts with an
 * empty capability set, except for the rights in `rights`; the caller grants anything else with
 * `cap_grant`. When any step fails, the half built process is destroyed.
 * @param image ELF image in the memory of the caller.
 * @param size Image size in bytes.
 * @param name NUL terminated process name; its length is measured here.
 * @param notify_exit true to receive a `PROCESS_EXITED` message when the process ends; the call fails when the caller already has `MAX_NOTIFIED_CHILDREN` such children running or unread.
 * @param rights The service label of the new process and the labels it may talk to, applied before its first thread can run; null leaves the process with no label and no IPC capability. The caller must hold a grantable `IPC` capability for the label and for each label of the list.
 * @return The new process id (the id of its first thread), or `NO_PROCESS` when the caller may
 *         not spawn, the image is not a loadable ELF of this architecture, ids or memory ran
 *         out, a pointer is bad, or the rights are invalid (no label, too many labels, a label
 *         the caller cannot give, or the derivation tree is full).
 */
inline uint32_t spawn(const void *image, size_t size, const char *name, bool notify_exit = false,
					  const abi::service::SpawnRights *rights = nullptr) {
	elf::Image parsed;
	constexpr uint32_t IMAGE_LIMIT =
		abi::executable::USER_SPACE_LIMIT - static_cast<uint32_t>(abi::process::PENDING_STACK_BYTES);
	if (!elf::parse(image, size, IMAGE_LIMIT, parsed)) return NO_PROCESS;

	uint32_t handle = proc_create(name, notify_exit, rights, abi::executable::USER_SPACE_LIMIT);
	if (handle == abi::process::NO_PENDING_PROCESS) return NO_PROCESS;

	uint32_t pid = NO_PROCESS;
	if (load_segments(handle, static_cast<const uint8_t *>(image), parsed))
		pid = proc_start(handle, parsed.entry);
	if (pid == NO_PROCESS) proc_discard(handle);
	return pid;
}

} // namespace cock::user

#endif // !COCK_USER_SPAWN_HPP
