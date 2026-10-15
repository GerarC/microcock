#include <cock/core/syscall/syscalls.hpp>
#include <stdint.h>

namespace cock::core::syscall {

uintptr_t sys_sched_yield(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t){
	return 0;
}

} // namespace cock::core::syscall
