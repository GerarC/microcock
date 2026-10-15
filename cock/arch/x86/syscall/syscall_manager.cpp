#include <cock/utils/logger.hpp>
#include <cock/arch/x86/syscall/syscall_nums.hpp>
#include <cock/core/syscall/syscall_manager.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <stddef.h>

namespace cock::core::syscall {

using arch::x86::SyscallNum;

constexpr int MAX_X86_SYSCALLS = 0x190;

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

    syscall_table[SyscallNum::SYS_CAP_GRANT] = sys_cap_grant;
    syscall_table[SyscallNum::SYS_IRQ_BIND] = sys_irq_bind;
    syscall_table[SyscallNum::SYS_IRQ_ACK] = sys_irq_ack;
    syscall_table[SyscallNum::SYS_PROC_LOOKUP] = sys_proc_lookup;

    syscall_table[SyscallNum::SYS_SHM_CREATE] = sys_shm_create;
    syscall_table[SyscallNum::SYS_SHM_SHARE] = sys_shm_share;
    syscall_table[SyscallNum::SYS_SHM_MAP] = sys_shm_map;
    syscall_table[SyscallNum::SYS_SHM_RELEASE] = sys_shm_release;
    syscall_table[SyscallNum::SYS_MODULE_OPEN] = sys_module_open;
    syscall_table[SyscallNum::SYS_MODULE_INDEX] = sys_module_index;
    syscall_table[SyscallNum::SYS_SPAWN] = sys_spawn;
    syscall_table[SyscallNum::SYS_PROC_ALIVE] = sys_proc_alive;
    syscall_table[SyscallNum::SYS_IPC_TRY_RECV] = sys_ipc_try_recv;
    syscall_table[SyscallNum::SYS_IPC_RECV_TIMEOUT] = sys_ipc_recv_timeout;
    syscall_table[SyscallNum::SYS_SLEEP] = sys_sleep;
    syscall_table[SyscallNum::SYS_THREAD_CREATE] = sys_thread_create;
    syscall_table[SyscallNum::SYS_FUTEX_WAIT] = sys_futex_wait;
    syscall_table[SyscallNum::SYS_FUTEX_WAKE] = sys_futex_wake;
    syscall_table[SyscallNum::SYS_GETTID] = sys_gettid;
}

SystemCall SyscallManager::getSyscall(size_t code){
    if (code >= MAX_X86_SYSCALLS || syscall_table[code] == nullptr) {
            utils::Logger::trace("Unimplemented Syscall %d called!", code);
            return nullptr;
    }
    return syscall_table[code];
}

} // namespace cock::core::syscall
