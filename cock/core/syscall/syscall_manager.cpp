// ===== cock/core/task/syscall_manager.cpp =====
#include "cock/core/syscall/syscall_manager.hpp"
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/syscall/syscall_nums.hpp>
#include <cock/driver/vga.hpp>
#include <cock/utils/logger.hpp>

namespace cock::core::syscall {

using namespace task;
using namespace cock::syscall;
using cock::utils::Logger;

uintptr_t SyscallManager::handle(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    Thread* current = Scheduler::getCurrentThread();

    if (!current) {
        Logger::error("Syscall %d called but no current thread!", num);
        return (uintptr_t)-1;
    }

    switch (num) {
        case SYS_EXIT:
            Logger::trace("Syscall: EXIT (Thread %d, Code: %d)", current->getId(), arg1);
            Scheduler::exitCurrentThread(ThreadResult::success());
            return 0;

        case SYS_WRITE: {
            if (arg1 == 1 || arg1 == 2) {
                const char* user_buf = reinterpret_cast<const char*>(arg2);
                
                cock::driver::vga_instance->write(user_buf, arg3);
                return arg3;
            }
            return (uintptr_t)-1; 
        }

        case SYS_GETPID:
            return current->getId();

        case SYS_SCHED_YIELD:
            Scheduler::yield(); 
            return 0;

        default:
            return (uintptr_t)-1;
    }
}

} // namespace cock::core::task
