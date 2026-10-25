#include <cock/utils/logger.hpp>
#include <cock/arch/x86/syscall/syscall_nums.hpp>
#include <cock/core/syscall/syscall_manager.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/thread_metrics.hpp>
#include <stddef.h>

namespace cock::core::syscall {

using arch::x86::SyscallNum;

constexpr int MAX_X86_SYSCALLS = arch::x86::SYSCALL_TABLE_SIZE;

SystemCall syscall_table[MAX_X86_SYSCALLS] = {nullptr};

void SyscallManager::init() {
    for (SystemCall &entry : syscall_table) {
        entry = sys_not_implemented;
    }


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
    syscall_table[SyscallNum::SYS_CAP_REVOKE] = sys_cap_revoke;
    syscall_table[SyscallNum::SYS_CAP_ACCEPT] = sys_cap_accept;
    syscall_table[SyscallNum::SYS_SERVICE_SET] = sys_service_set;
    syscall_table[SyscallNum::SYS_EP_CREATE] = sys_ep_create;
    syscall_table[SyscallNum::SYS_EP_BIND] = sys_ep_bind;
    syscall_table[SyscallNum::SYS_EP_SEND] = sys_ep_send;
    syscall_table[SyscallNum::SYS_EP_REPLY] = sys_ep_reply;
    syscall_table[SyscallNum::SYS_EP_FORWARD] = sys_ep_forward;
    syscall_table[SyscallNum::SYS_EP_CALL] = sys_ep_call;
    syscall_table[SyscallNum::SYS_IRQ_BIND] = sys_irq_bind;
    syscall_table[SyscallNum::SYS_IRQ_ACK] = sys_irq_ack;
    syscall_table[SyscallNum::SYS_MODULE_PROCESS] = sys_module_process;

    syscall_table[SyscallNum::SYS_SHM_CREATE] = sys_shm_create;
    syscall_table[SyscallNum::SYS_SHM_SHARE] = sys_shm_share;
    syscall_table[SyscallNum::SYS_SHM_MAP] = sys_shm_map;
    syscall_table[SyscallNum::SYS_SHM_RELEASE] = sys_shm_release;
    syscall_table[SyscallNum::SYS_MODULE_OPEN] = sys_module_open;
    syscall_table[SyscallNum::SYS_MODULE_INDEX] = sys_module_index;
    syscall_table[SyscallNum::SYS_PROC_CREATE] = sys_proc_create;
    syscall_table[SyscallNum::SYS_PROC_MAP] = sys_proc_map;
    syscall_table[SyscallNum::SYS_PROC_WRITE] = sys_proc_write;
    syscall_table[SyscallNum::SYS_PROC_PROTECT] = sys_proc_protect;
    syscall_table[SyscallNum::SYS_PROC_START] = sys_proc_start;
    syscall_table[SyscallNum::SYS_PROC_DISCARD] = sys_proc_discard;
    syscall_table[SyscallNum::SYS_PROC_ALIVE] = sys_proc_alive;
    syscall_table[SyscallNum::SYS_IPC_TRY_RECV] = sys_ipc_try_recv;
    syscall_table[SyscallNum::SYS_IPC_RECV_TIMEOUT] = sys_ipc_recv_timeout;
    syscall_table[SyscallNum::SYS_SLEEP] = sys_sleep;
    syscall_table[SyscallNum::SYS_THREAD_CREATE] = sys_thread_create;
    syscall_table[SyscallNum::SYS_FUTEX_WAIT] = sys_futex_wait;
    syscall_table[SyscallNum::SYS_FUTEX_WAKE] = sys_futex_wake;
    syscall_table[SyscallNum::SYS_GETTID] = sys_gettid;
    syscall_table[SyscallNum::SYS_THREAD_PROCESS] = sys_thread_process;
    syscall_table[SyscallNum::SYS_PROC_STATE] = sys_proc_state;
    syscall_table[SyscallNum::SYS_PROC_EXEC] = sys_proc_exec;
    syscall_table[SyscallNum::SYS_PROC_FORK] = sys_proc_fork;
    syscall_table[SyscallNum::SYS_PROC_SIGNAL] = sys_proc_signal;
    syscall_table[SyscallNum::SYS_IPC_CALL] = sys_ipc_call;
    syscall_table[SyscallNum::SYS_IPC_ACCEPT] = sys_ipc_accept;
    syscall_table[SyscallNum::SYS_IPC_REPLY] = sys_ipc_reply;
    syscall_table[SyscallNum::SYS_IPC_REPLY_WAIT] = sys_ipc_reply_wait;
    syscall_table[SyscallNum::SYS_IPC_DEFER] = sys_ipc_defer;
    syscall_table[SyscallNum::SYS_IPC_REPLY_TO] = sys_ipc_reply_to;
    if constexpr (task::SCHED_METRICS_ENABLED)
        syscall_table[SyscallNum::SYS_SCHED_METRICS] = sys_sched_metrics;
}

SystemCall SyscallManager::getSyscall(size_t code){
    if (code >= MAX_X86_SYSCALLS || syscall_table[code] == sys_not_implemented) {
        utils::Logger::trace("Unimplemented Syscall %d called!", code);
        return sys_not_implemented;
    }
    return syscall_table[code];
}

} // namespace cock::core::syscall
