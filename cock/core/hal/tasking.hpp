#ifndef CORE_HAL_TASKING_HPP
#define CORE_HAL_TASKING_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::hal {

uintptr_t prepare_thread_stack(void *stack_base, size_t stack_size,
							   void *wrapper_function, void *user_arg);

uintptr_t prepare_user_thread_stack(void *kernel_stack_base,
									size_t kernel_stack_size,
									void *user_stack_base,
									size_t user_stack_size, void *entry_point,
									void *user_arg);
void yield();

void set_kernel_stack(uintptr_t stack_top);
void update_thread_entry(uintptr_t context_ptr, void* new_entry);
void switch_address_space(uintptr_t pd_phys);
uintptr_t get_current_address_space();


} // namespace cock::core::hal

#endif // !CORE_HAL_TASKING_HPP
