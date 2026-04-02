#include <cock/core/syscall/syscalls.hpp>
#include <stddef.h>

namespace cock::core::syscall {

class SyscallManager {
public:
	static void init();
    static SystemCall getSyscall(size_t code);
};

} // namespace cock::arch::x86
