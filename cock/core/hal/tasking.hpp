/** @file
 * @brief HAL contract for building and manipulating thread contexts, switching
 * address spaces and granting hardware access to processes.
 *
 * The core calls these functions without knowing the architecture. A thread
 * context is an opaque saved register frame referenced by a uintptr_t that
 * points into the thread kernel stack.
 */
#ifndef CORE_HAL_TASKING_HPP
#define CORE_HAL_TASKING_HPP

#include <cock/core/memory/address.hpp>
#include <hal/cpu.hpp>
#include <hal/process_context.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::hal {

/**
 * @brief Prepares a fresh stack for a Ring 0 Kernel thread.
 * @param stack_base Pointer to the bottom of the allocated stack.
 * @param stack_size Size of the allocated stack.
 * @param wrapper_function The entry wrapper that will execute the user
 * function.
 * @param user_arg The actual function pointer the thread should execute.
 * @return The initial stack pointer (ESP) ready for a context switch.
 * @note The stack must be large enough for one saved context. The thread
 * starts with interrupts enabled. No validation is done, the caller owns
 * the memory.
 */
uintptr_t prepare_thread_stack(void *stack_base, size_t stack_size,
							   void *wrapper_function, void *user_arg);

/**
 * @brief Prepares stacks for a Ring 3 User thread, setting up iret structures.
 * @param kernel_stack_base Bottom of the kernel stack of the thread, where the
 * saved context is placed.
 * @param kernel_stack_size Size of the kernel stack.
 * @param user_stack_base Bottom of the user stack, already mapped in the
 * address space the thread will run in.
 * @param user_stack_size Size of the user stack.
 * @param entry_point User address where execution starts.
 * @param user_arg Value handed to the entry point as its first argument.
 * @return Saved context ready to be resumed, which runs in Ring 3 with
 * interrupts enabled.
 * @note Pointers are not validated here, the caller has already checked them.
 */
uintptr_t prepare_user_thread_stack(void *kernel_stack_base,
									size_t kernel_stack_size,
									void *user_stack_base,
									size_t user_stack_size, void *entry_point,
									void *user_arg);

/**
 * @brief Tells the CPU which kernel stack to use on the next entry from
 * Ring 3 (the TSS esp0 on x86).
 * @param stack_top Top of the kernel stack of the thread about to run.
 * @note Called by the scheduler on every switch to a user thread.
 */
void set_kernel_stack(uintptr_t stack_top);

/**
 * @brief Modifies the instruction pointer (EIP) of a dormant thread context.
 * @param context_ptr Saved context of a thread that is not running.
 * @param new_entry New address to resume at.
 */
void update_thread_entry(uintptr_t context_ptr, void *new_entry);

/**
 * @brief Sets the value a dormant user thread finds in its first argument register when it
 * starts running (`EAX` on x86).
 * @param context_ptr Saved context of a thread that has never run.
 * @param value The value; truncated to the register width.
 */
void set_thread_argument(uintptr_t context_ptr, uintptr_t value);

/**
 * @brief Moves the saved instruction pointer back over the system call
 * instruction, so the call is executed again when the thread resumes.
 * @param context_ptr Saved context of a thread that is inside a call.
 * @note Used to restart a call that had to wait.
 */
void rewind_syscall(uintptr_t context_ptr);

/** @brief Number of extra registers, besides the result, a call returns in. */
constexpr size_t SYSCALL_FRAME_VALUES = 5;

/**
 * @brief The registers a system call returns in, in a neutral form.
 */
struct SyscallFrame {
	uintptr_t result; /**< Primary return register. */
	uintptr_t values[SYSCALL_FRAME_VALUES]; /**< Additional return registers,
											   in argument order. */
};

/**
 * @brief Reads and writes the registers a system call returns in: the result
 * and five values, in the saved context of a thread that is inside a call.
 * @param context_ptr Saved context of the thread.
 * @param frame Receives the registers (read) or supplies them (write).
 * @note The context must belong to a thread that is not running on the CPU.
 */
void read_syscall_frame(uintptr_t context_ptr, SyscallFrame &frame);

/**
 * @brief Writes the return registers of a system call into a saved context.
 * @param context_ptr Saved context of the thread.
 * @param frame Registers to store. Values are truncated to the register width.
 * @see read_syscall_frame
 */
void write_syscall_frame(uintptr_t context_ptr, const SyscallFrame &frame);

/**
 * @brief Performs a hardware page directory switch (writes to CR3 on x86).
 * @param root Physical address of the page directory to activate.
 * @note The root must be a valid directory that maps the kernel.
 */
inline void switch_address_space(memory::PhysAddr root) {
	write_page_table_root(root.raw());
}

/**
 * @brief Retrieves the current physical address of the page directory (CR3 on
 * x86).
 * @return Physical address of the active page directory.
 */
inline memory::PhysAddr get_current_address_space() {
	return memory::PhysAddr(read_page_table_root());
}

/**
 * @brief Initializes the architecture state of a new process: frees any
 * previous permission map and assigns a fresh context version.
 * @param context Context embedded in the process.
 */
void init_process_context(ArchProcessContext &context);

/**
 * @brief Frees what the architecture state of a process owns and leaves it
 * with no hardware access.
 * @param context Context embedded in the process. Safe to call twice.
 */
void release_process_context(ArchProcessContext &context);

/**
 * @brief Grants a thread access to specific hardware resources (e.g., I/O ports
 * via TSS IOPM).
 * @param context Context of the process that receives the grant.
 * @param resource_id First resource, a port number on x86.
 * @param size Number of consecutive resources.
 * @return True when granted. False when the range is empty, out of bounds,
 * overlaps resources the kernel keeps for itself (PIC and PIT ports) or the
 * permission map cannot be allocated.
 * @note Allocates from the kernel heap. Bumps the context version so the next
 * apply_process_context reloads the map.
 */
bool grant_hardware_resource(ArchProcessContext &context, uint32_t resource_id,
							 uint32_t size);

/**
 * @brief Takes back access to hardware resources that grant_hardware_resource
 * gave (the ports in the permission map on x86).
 * @param context Context of the process that loses the access.
 * @param resource_id First resource, a port number on x86.
 * @param size Number of consecutive resources. A range that is empty, outside
 * the bounds or was never granted changes nothing.
 * @note Bumps the context version so the next apply_process_context reloads the
 * map. When the process is the one running, the caller applies the context.
 */
void revoke_hardware_resource(ArchProcessContext &context, uint32_t resource_id,
							  uint32_t size);

/**
 * @brief Applies the architecture-specific context (like IOPM) to the CPU.
 * @param context Context of the process about to run.
 * @note Called by the scheduler when switching processes.
 */
void apply_process_context(const ArchProcessContext &context);

} // namespace cock::core::hal

#endif // !CORE_HAL_TASKING_HPP
