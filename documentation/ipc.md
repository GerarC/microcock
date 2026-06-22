# Inter-Process Communication (IPC) & Syscalls

The core philosophy of MicroCOCK is moving traditional monolithic kernel drivers into userland. This requires a robust system call interface and lightning-fast message passing.

## System Calls
System calls are triggered via the software interrupt `int 0x80`. The CPU traps this vector and forwards execution to `isr_handler`, which reads the syscall index from the `EAX` register.

The `SyscallManager` maps these indices to specific handler functions in the kernel core.
```cpp
void SyscallManager::init() {
    syscall_table[SyscallNum::SYS_EXIT] = sys_exit;
    syscall_table[SyscallNum::SYS_READ] = sys_read;
    syscall_table[SyscallNum::SYS_WRITE] = sys_write;
    syscall_table[SyscallNum::SYS_MMAP] = sys_mmap;
    syscall_table[SyscallNum::SYS_IPC_SEND] = sys_ipc_send;
    syscall_table[SyscallNum::SYS_IPC_RECV] = sys_ipc_recv; 
}
```

## IPC Message Passing
Threads communicate using a highly efficient, lock-free `RingBuffer`. Every thread maintains an internal inbox queue holding up to 16 `Message` structures.

### Sending (`sys_ipc_send`)
When Thread A sends a message to Thread B, `IPC::send` pushes the message directly into Thread B's inbox. If Thread B was blocked waiting for a message (`WAITING_MSG`), it is immediately awakened, set to `READY`, and placed back into the scheduler.

```cpp
void IPC::send(uint32_t to_pid, Message message) {
    hal::block_interruptions();
    Thread *target = TaskManager::getThreadByPID(to_pid); 
    // ... validation ...
    target->receiveMessage(message);

    if (target->getState() == ThreadState::WAITING_MSG) {
        target->setState(ThreadState::READY);
        Scheduler::addThread(target); 
        // ...
    }
}
```

### Receiving (`sys_ipc_recv`)
If a thread attempts to receive an IPC message but its inbox is empty, the `IPC::receive` routine safely transitions the thread's state to `WAITING_MSG` and yields the CPU. The thread will not be scheduled again until another process sends a message to its inbox, ensuring zero CPU cycles are wasted on active polling.
