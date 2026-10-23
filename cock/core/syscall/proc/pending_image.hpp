/** @file
 * @brief Checks and address space switching shared by the `SYS_PROC_*` calls
 * that build the image of a pending process.
 */
#ifndef CORE_SYSCALL_PROC_PENDING_IMAGE_HPP
#define CORE_SYSCALL_PROC_PENDING_IMAGE_HPP

#include <cock/abi/process.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/memory/address.hpp>
#include <cock/core/task/pending_process.hpp>
#include <cock/core/task/process.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::syscall {

/** @brief Raw result of a system call that failed. */
constexpr uintptr_t PROC_FAILURE = static_cast<uintptr_t>(-1);
/** @brief Raw result of a system call that worked. */
constexpr uintptr_t PROC_SUCCESS = 0;

/**
 * @brief Makes the address space of a pending process the active one for the
 * lifetime of the object, with interrupts blocked, and restores the previous one
 * on destruction.
 */
class PendingSpaceScope {
  public:
	/**
	 * @brief Switches to the address space of a process.
	 * @param process Process whose space becomes active; it must not be running.
	 */
	explicit PendingSpaceScope(const task::Process &process);
	/** @brief Switches back to the address space that was active before. */
	~PendingSpaceScope();
	/** @brief Copying is forbidden: the scope owns a switch. */
	PendingSpaceScope(const PendingSpaceScope &) = delete;
	/** @brief Assignment is forbidden: the scope owns a switch. */
	PendingSpaceScope &operator=(const PendingSpaceScope &) = delete;

  private:
	hal::InterruptGuard guard; /**< Interrupts stay blocked while switched. */
	memory::PhysAddr previous; /**< The address space to go back to. */
};

/**
 * @brief Finds the pending process of a handle for the calling process.
 * @param handle Handle given by the program, untrusted.
 * @return The entry, or null when there is no calling thread or the handle is not
 * a pending process of the caller.
 */
task::PendingProcesses::Entry *pending_of_caller(uintptr_t handle);

/**
 * @brief Tells whether an address range can hold part of the image of a process.
 * @param process The pending process.
 * @param address Start of the range, untrusted.
 * @param bytes Length of the range in bytes, non zero.
 * @return True when the range does not wrap, does not touch the first page and ends
 * at or below the start of the user stack of the process.
 */
bool image_range_fits(const task::Process &process, uintptr_t address, uint64_t bytes);

/**
 * @brief Tells whether a page range given to `SYS_PROC_MAP` or `SYS_PROC_PROTECT`
 * is valid.
 * @param process The pending process.
 * @param address Start of the range, untrusted, must be page aligned.
 * @param pages Page count, untrusted, non zero.
 * @return True when the start is aligned and `image_range_fits` accepts the range.
 */
bool image_pages_fit(const task::Process &process, uintptr_t address, uintptr_t pages);

} // namespace cock::core::syscall

#endif // !CORE_SYSCALL_PROC_PENDING_IMAGE_HPP
