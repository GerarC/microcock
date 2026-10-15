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
 */
uintptr_t prepare_thread_stack(void *stack_base, size_t stack_size,
							   void *wrapper_function, void *user_arg);

/**
 * @brief Prepares stacks for a Ring 3 User thread, setting up iret structures.
 */
uintptr_t prepare_user_thread_stack(void *kernel_stack_base,
									size_t kernel_stack_size,
									void *user_stack_base,
									size_t user_stack_size, void *entry_point,
									void *user_arg);
void set_kernel_stack(uintptr_t stack_top);

/**
 * @brief Modifies the instruction pointer (EIP) of a dormant thread context.
 */
void update_thread_entry(uintptr_t context_ptr, void *new_entry);

/**
 * @brief Makes a saved context execute its system call instruction again when
 * it resumes.
 */
void rewind_syscall(uintptr_t context_ptr);

/**
 * @brief Performs a hardware page directory switch (writes to CR3 on x86).
 */
inline void switch_address_space(memory::PhysAddr root) {
	write_page_table_root(root.raw());
}

/**
 * @brief Retrieves the current physical address of the page directory (CR3 on
 * x86).
 */
inline memory::PhysAddr get_current_address_space() {
	return memory::PhysAddr(read_page_table_root());
}

/**
 * @brief Resets a process context so it grants no hardware resource.
 */
void init_process_context(ArchProcessContext &context);

/**
 * @brief Grants a thread access to specific hardware resources (e.g., I/O ports
 * via TSS IOPM).
 */
bool grant_hardware_resource(ArchProcessContext &context, uint32_t resource_id,
							 uint32_t size);

/**
 * @brief Applies the architecture-specific context (like IOPM) to the CPU.
 */
void apply_process_context(const ArchProcessContext &context);

} // namespace cock::core::hal

#endif // !CORE_HAL_TASKING_HPP
