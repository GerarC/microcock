#include <cock/utils/logger.hpp>
#include <cock/arch/x86/syscall/syscall_nums.hpp>
#include <cock/core/syscall/syscall_manager.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <stddef.h>

namespace cock::core::syscall {

using arch::x86::SyscallNum;

constexpr int MAX_X86_SYSCALLS = 0x180;

SystemCall syscall_table[MAX_X86_SYSCALLS] = {nullptr};

void SyscallManager::init() {

	syscall_table[SyscallNum::SYS_EXIT] = sys_exit;

	syscall_table[SyscallNum::SYS_READ] = sys_read;
	syscall_table[SyscallNum::SYS_WRITE] = sys_write;

	syscall_table[SyscallNum::SYS_REQUEST_RESOURCE] = sys_request_resource;

	syscall_table[SyscallNum::SYS_GETPID] = sys_getpid;
	syscall_table[SyscallNum::SYS_SCHED_YIELD] = sys_sched_yield;

    syscall_table[SyscallNum::SYS_MMAP] = sys_mmap;
    syscall_table[SyscallNum::SYS_MUNMAP] = sys_munmap;

    syscall_table[SyscallNum::SYS_IPC_SEND] = sys_ipc_send;
    syscall_table[SyscallNum::SYS_IPC_RECV] = sys_ipc_recv;
}

SystemCall SyscallManager::getSyscall(size_t code){
    if (code >= MAX_X86_SYSCALLS || syscall_table[code] == nullptr) {
            utils::Logger::error("Unimplemented Syscall %d called!", code);
            return nullptr;
    }
    return syscall_table[code];
}

} // namespace cock::core::syscall
