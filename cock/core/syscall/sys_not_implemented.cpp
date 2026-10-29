#include <cock/core/syscall/syscalls.hpp>
#include <stdint.h>

namespace cock::core::syscall {

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);

uintptr_t sys_not_implemented(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
							  uintptr_t, uintptr_t) {
	return FAILURE;
}

} // namespace cock::core::syscall
