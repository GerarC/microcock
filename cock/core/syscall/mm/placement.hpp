#ifndef CORE_SYSCALL_PLACEMENT_HPP
#define CORE_SYSCALL_PLACEMENT_HPP

#include <cock/core/memory/address.hpp>
#include <cock/core/task/thread.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::syscall {

uintptr_t user_image_limit(const task::Thread &thread);
memory::VirtAddr find_user_placement(task::Thread &current, size_t pages);

} // namespace cock::core::syscall

#endif // !CORE_SYSCALL_PLACEMENT_HPP
