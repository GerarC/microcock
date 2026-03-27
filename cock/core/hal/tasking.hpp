#ifndef CORE_HAL_TASKING_HPP
#define CORE_HAL_TASKING_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::hal {

uintptr_t prepare_thread_stack(void* stack_base, size_t stack_size, void* wrapper_function, void* user_arg);
void yield();

}

#endif // !CORE_HAL_TASKING_HPP
