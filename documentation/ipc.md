# Inter-Process Communication (IPC) & Syscalls

The core philosophy of MicroCOCK is moving traditional monolithic kernel drivers into userland. This requires a robust system call interface and cheap message passing.

## System Calls
System calls are triggered via the software interrupt `int 0x80`. The vector has a descriptor privilege level of 3, so a program can raise it; every other vector answers a program with a protection fault. The architecture reads the syscall number from `EAX` and the six arguments from `EBX`, `ECX`, `EDX`, `ESI`, `EDI` and `EBP`, packs them into a `SyscallRequest` and passes it to the core.

The `SyscallManager` maps numbers to handler functions in `cock/core/syscall/`, one file per call.

```cpp
void SyscallManager::init() {
	syscall_table[SyscallNum::SYS_EXIT] = sys_exit;
	syscall_table[SyscallNum::SYS_READ] = sys_read;
	syscall_table[SyscallNum::SYS_WRITE] = sys_write;
	syscall_table[SyscallNum::SYS_REQUEST_RESOURCE] = sys_request_resource;
	syscall_table[SyscallNum::SYS_MMAP] = sys_mmap;
	syscall_table[SyscallNum::SYS_IPC_SEND] = sys_ipc_send;
	syscall_table[SyscallNum::SYS_IPC_RECV] = sys_ipc_recv;
	syscall_table[SyscallNum::SYS_CAP_GRANT] = sys_cap_grant;
	syscall_table[SyscallNum::SYS_IRQ_BIND] = sys_irq_bind;
	syscall_table[SyscallNum::SYS_IRQ_ACK] = sys_irq_ack;
	syscall_table[SyscallNum::SYS_PROC_LOOKUP] = sys_proc_lookup;
	...
}
```

The full table with the numbers is in the [README](../README.md). The value a handler returns goes back in `EAX`; `-1` means failure. An unknown number returns `-1` as well.

### Pointers from userland
No handler trusts a pointer. Before reading or writing user memory, a handler asks the VMM whether the whole range is mapped, accessible from Ring 3, writable when needed, and below the user-space limit of the calling process. A kernel address, an unmapped page or a range that wraps around makes the call fail with `-1` and nothing else happens.

### Blocking
A call that has to wait asks the scheduler to restart it (see [task management](task_management.md#blocking-system-calls)). The kernel never blocks inside a handler.

## IPC Message Passing
Threads communicate using a `RingBuffer`. Every thread keeps an inbox with 16 slots (15 usable) holding `Message` structures. Delivery never blocks the sender.

### The message
```cpp
typedef struct message_t {
	uint32_t sender_pid;
	uint32_t target_pid;
	MessageType type;
	union {
		uint32_t argv[MAX_ARGUMENTS];
		struct { uint32_t arg0; uint32_t arg1; uint32_t arg2;
				 uint32_t arg3; uint32_t arg4; uint32_t arg5; } args;
		void *ptr;
	} payload;
} Message;
```

The kernel writes `sender_pid` and `target_pid` itself on delivery; whatever the sender put there is replaced. The message types are:

| Type | Meaning |
|------|---------|
| `NONE` | Empty message; the display server treats it as a request to clear the screen |
| `SYS_PING`, `SYS_PONG`, `SYS_DEBUG`, `SYS_REGISTER_MAILBOX` | Reserved for tests, debugging and future use |
| `VIDEO_WRITE_CHAR` | Write one character (`arg0`) on the display server |
| `VIDEO_WRITE_STR` | Write `arg0` bytes that follow in `arg1` and the next arguments |
| `IRQ_NOTIFICATION` | The kernel tells a driver that its interrupt line fired (`arg0` is the line) |
| `CAPABILITY_GRANTED` | The root server tells a driver that its capabilities are in place (`arg0` is a peer process id) |

### Sending (`sys_ipc_send`)
`IPC::deliver` validates the target, stamps the message and pushes it into the inbox of the target. If the target was blocked waiting for a message (`WAITING_MSG`), it is set to `READY` and put back in the scheduler. The call fails, and the sender is told, when the target does not exist or its inbox is full.

```cpp
bool IPC::deliver(uint32_t to_pid, Message message) {
	uintptr_t flags = hal::save_and_block_interruptions();
	Thread *target = TaskManager::getThreadByPID(to_pid);
	Thread *sender = Scheduler::getCurrentThread();
	...
	if (target->getState() == ThreadState::WAITING_MSG) {
		target->setState(ThreadState::READY);
		Scheduler::addThread(target);
	}
	hal::restore_interruptions(flags);
	return true;
}
```

### Receiving (`sys_ipc_recv`)
If the inbox has a message, it is copied to the buffer of the caller. If it is empty, the handler sets the thread to `WAITING_MSG` and asks the scheduler for a restart. The thread is not scheduled again until a message arrives, so no cycles are wasted on polling, and when it wakes up it simply runs the system call again and finds the message.

## Interrupts as messages
A driver in userland handles a hardware interrupt like this:

1. it calls `SYS_IRQ_BIND` with the line (it needs an `IRQ` capability) and the kernel unmasks the line,
2. when the line fires, the kernel **masks it** and sends an `IRQ_NOTIFICATION` message to the owner,
3. the driver reads its device, then calls `SYS_IRQ_ACK`, and the kernel unmasks the line again.

The timer line and the PIC cascade line are reserved and cannot be bound. When the owner dies, the kernel masks and releases its lines. The keyboard driver in `userland/kbd` is the reference.
