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
	syscall_table[SyscallNum::SYS_SHM_CREATE] = sys_shm_create;
	syscall_table[SyscallNum::SYS_SHM_SHARE] = sys_shm_share;
	syscall_table[SyscallNum::SYS_SHM_MAP] = sys_shm_map;
	syscall_table[SyscallNum::SYS_SHM_RELEASE] = sys_shm_release;
	syscall_table[SyscallNum::SYS_MODULE_OPEN] = sys_module_open;
	...
}
```

The shared memory calls (`SYS_SHM_*`, numbers `0x174` to `0x177`) move bulk data between processes, which a message of six arguments cannot do: a process creates an object, shares it with the pid of another process by IPC, and the other process maps it. See [memory management](memory_management.md).

`SYS_MODULE_OPEN` (`0x178`) gives a process holding the `BOOT_MODULE` capability the id of a data boot module, as a read only shared memory object. `SYS_PROC_CREATE` (`0x17A`), `SYS_PROC_MAP` (`0x18C`), `SYS_PROC_WRITE` (`0x18D`), `SYS_PROC_PROTECT` (`0x18E`), `SYS_PROC_START` (`0x18F`) and `SYS_PROC_DISCARD` (`0x190`) build and start a process, and the library function `cock::user::spawn` uses them to start a program from an image in the memory of the caller (see [task management](task_management.md#spawn)) and `SYS_PROC_ALIVE` (`0x17B`) says whether a process id is alive. `SYS_MODULE_INDEX` (`0x179`) gives the index of a data module to a process that holds grant authority, so `init` can grant the capability for one module.

The full table with the numbers is in the [README](../README.md). The value a handler returns goes back in `EAX`; `-1` means failure. An unknown number returns `-1` as well.

### Pointers from userland
No handler trusts a pointer. Before reading or writing user memory, a handler asks the VMM whether the whole range is mapped, accessible from Ring 3, writable when needed, and below the user-space limit of the calling process. A kernel address, an unmapped page or a range that wraps around makes the call fail with `-1` and nothing else happens.

### Blocking
A call that has to wait asks the scheduler to restart it (see [task management](task_management.md#blocking-system-calls)). The kernel never blocks inside a handler.

## IPC Message Passing
Threads communicate using a `RingBuffer`. Every thread keeps an inbox that holds 16 (`THREAD_INBOX_CAPACITY`) of the `Message` structures. Delivery never blocks the sender.

### Inboxes belong to threads
The inbox, the pending interrupt notifications and the interrupt lines a program binds belong to one **thread**, not to the process. `sender_pid` in a message is the id of the sending thread. A process id given to `SYS_IPC_SEND` or `SYS_PROC_ALIVE` stands for its first thread, and a thread id works too. `SYS_IRQ_BIND` and `SYS_IRQ_ACK` are tied to the thread that bound the line, so a program that binds a line in one thread and receives in another never sees the notification: bind and receive in the same thread, and hand the work to other threads from there.

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

The kernel writes `sender_pid` and `target_pid` itself on delivery; whatever the sender put there is replaced. The sender of a message produced by the kernel is `KERNEL_SENDER` (`0`, defined in `shared/include/cock/ipc/message.hpp`). The message types are:

| Type | Meaning |
|------|---------|
| `NONE` | Empty message; the display server treats it as a request to clear the screen |
| `SYS_PING`, `SYS_PONG`, `SYS_DEBUG`, `SYS_REGISTER_MAILBOX` | Reserved for tests, debugging and future use |
| `OUTPUT_WRITE_CHAR` | Write one character (`arg0`) on the display server |
| `CONSOLE_KEY` | Keyboard driver to console server (value 30): `arg0` is the typed character; the console accepts it only from the process named `kbd`; see [console](../userland/console.md) |
| `OUTPUT_WRITE_STR` | Write `arg0` bytes that follow in `arg1` and the next arguments |
| `PROCESS_EXITED` | The kernel tells the spawner that a process it asked about ended (`arg0` process id, `arg1` exit status, `0xFFFFFFFF` when an exception ended it). It comes from a per-process record, not from the inbox, so it is never lost; only the main thread of the spawner receives it. Only the kernel produces it; a program that tries to send one gets `-1` |
| `IRQ_NOTIFICATION` | The kernel tells a driver that its interrupt line fired (`arg0` is the line). Only the kernel produces it, with `KERNEL_SENDER` as sender; a program that tries to send one gets `-1` |
| `FS_CONNECT`, `FS_OPEN`, `FS_READ`, `FS_STAT`, `FS_READDIR`, `FS_CLOSE`, `FS_REPLY`, `FS_WRITE`, `FS_UNLINK`, `FS_MKDIR`, `FS_RMDIR`, `FS_RENAME`, `FS_LINK` | The file server protocol (values 9 to 15, 20, 21 and 23 to 26); see [file system](../userland/filesystem.md) |
| `BLK_CONNECT`, `BLK_INFO`, `BLK_READ`, `BLK_REPLY`, `BLK_WRITE` | The block server protocol (values 16 to 19 and 22); see [block storage](../userland/block_storage.md) |
| `RTC_READ`, `RTC_REPLY` | The clock server protocol (values 27 and 28): `arg0` request id; the reply carries the status in `arg1` and the date and time packed in `arg2` and `arg3` (`shared/include/cock/abi/rtc.hpp`) |
| `CAPABILITY_GRANTED` | The root server tells a driver that its capabilities are in place (`arg0` is a peer process id). Only a process that holds a capability with the grant right can send it |

### Sending (`sys_ipc_send`)
Before delivering, the system call checks that the sender may use the message type: `IRQ_NOTIFICATION` and `PROCESS_EXITED` are refused to every program, and `CAPABILITY_GRANTED` is refused unless the sender holds grant authority. A server can therefore trust the type of a message it receives without checking who sent it. Then it checks the IPC capability (`mayTalk`): the sender and the target must be the same process, or one of them must hold an `IPC` capability that covers the service label of the other; see [capabilities](capabilities.md#the-ipc-capability). `SYS_IPC_CALL` and `SYS_SHM_SHARE` make the same check, and a refusal costs the caller nothing but a `-1`. `IPC::deliver` then validates the target, stamps the message and pushes it into the inbox of the target. If the target was blocked waiting for a message (`WAITING_MSG`), it is set to `READY` and put back in the scheduler. The call fails, and the sender is told, when the target does not exist or its inbox is full.

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
Pending interrupt notifications come first (see below): each one is delivered as an `IRQ_NOTIFICATION` message made on the spot. Otherwise, if the inbox has a message, it is copied to the buffer of the caller. If there is neither, the handler sets the thread to `WAITING_MSG` and asks the scheduler for a restart. The thread is not scheduled again until a message arrives, so no cycles are wasted on polling, and when it wakes up it simply runs the system call again and finds the message.

### Receiving without waiting (`sys_ipc_try_recv`)
`SYS_IPC_TRY_RECV` (`0x17C`) takes the same next item as `SYS_IPC_RECV` (a pending notification first, then the inbox) but never blocks: it returns `0` with the message copied, `1` when there is nothing, and `-1` when the pointer is not a writable user range. A program that has to notice a dead peer polls it together with `SYS_PROC_ALIVE`.

### Receiving with a time limit (`sys_ipc_recv_timeout`)
`SYS_IPC_RECV_TIMEOUT` (`0x17D`) takes `(message, milliseconds)`. It takes the next item like `SYS_IPC_RECV`; if there is none it waits at most that long. It returns `0` with the message copied, `1` when the time ran out, and `-1` for a bad pointer or a time above `0x7FFFFFFF`. A time of `0` never waits. The wait uses the same restart pattern as `SYS_IPC_RECV`: the thread is `WAITING_MSG` with a timeout armed, a message wakes it as usual, and the timer wakes it when the time is up; in both cases the call runs again, finds the message or finds the timeout flag and returns `1`. A message that arrives together with the timeout wins.

### Sleeping (`sys_sleep`)
`SYS_SLEEP` (`0x17E`) takes `(milliseconds)` and blocks the thread (`BLOCKED`, so messages do not wake it, they wait in the inbox) for at least that long and less than one tick more. `0` yields the CPU. A time above `0x7FFFFFFF` returns `-1`.

Threads and futexes (`SYS_THREAD_CREATE` `0x17F`, `SYS_FUTEX_WAIT` `0x180`, `SYS_FUTEX_WAKE` `0x181`, `SYS_GETTID` `0x182`) are described in [task management](task_management.md#threads).

Both calls are driven by `Timeouts` (see [task management](task_management.md#timeouts)). The clock is the 100 Hz tick, so the resolution is 10 ms and a time is rounded up to whole ticks.

## Synchronous calls
Besides the asynchronous messages, a thread can **call** another and wait for the answer. The request and the answer are four 32 bit words that travel in registers: nothing is copied through user memory, so there is no pointer to validate, and the kernel writes the answer straight into the saved registers of the thread that waits. The call is a separate channel: it carries no message type and does not go through `sender_may_use`; the server learns who called from the id the kernel stamps.

| Call | Number | Registers in | Registers out |
|------|--------|--------------|---------------|
| `SYS_IPC_CALL` | `0x183` | `EBX` target thread id, `ECX`, `EDX`, `ESI`, `EDI` the four words, `EBP` milliseconds | `EAX` status, `EBX` id of the replier, `ECX` to `EDI` the four words of the answer |
| `SYS_IPC_ACCEPT` | `0x184` | `EBP` milliseconds | `EAX` status, `EBX` id of the caller, `ECX` to `EDI` the four words of the request |
| `SYS_IPC_REPLY` | `0x185` | `ECX` to `EDI` the four words of the answer | `EAX` 0, or -1 when no call is being served |
| `SYS_IPC_REPLY_WAIT` | `0x186` | `ECX` to `EDI` the answer, `EBP` milliseconds | the same as `SYS_IPC_ACCEPT`; with no call being served it only waits |

| `SYS_IPC_DEFER` | `0x187` | none | `EAX` ticket of the call being served (the id of its caller), or -1 when none is being served |
| `SYS_IPC_REPLY_TO` | `0x188` | `EBX` ticket, `ECX` to `EDI` the four words of the answer | `EAX` 0, or -1 when this thread holds no detached call with that ticket |
| `SYS_SCHED_METRICS` | `0x189` | `EBX` address of an `abi::sched::Metrics` | `EAX` 0, or -1 for a bad buffer or a kernel built without `SCHED_METRICS=1` (see [task management](task_management.md#metrics)) |

`EAX` is `0` when the registers hold a call (or the answer), `1` when the time ran out, `2` when the wait ended because an asynchronous message or an interrupt notification is pending (fetch it with `SYS_IPC_TRY_RECV`), and `-1` on failure. The time is in milliseconds; `0xFFFFFFFF` means no limit and `0` never waits; anything else above `0x7FFFFFFF` is refused with `-1`.

`userland/lib/include/cock/user/call.hpp` wraps them (`ipc_call`, `ipc_accept`, `ipc_reply`, `ipc_reply_wait`, each with an optional time; `ipc_defer` returns the ticket or `NO_TICKET`, and `ipc_reply_to(ticket, words)`) and returns a `CallResult` with `status` (`OK`, `TIMED_OUT`, `MESSAGE_PENDING`, `FAILED`). A server is `accept`, then a loop of `reply_wait`. Because the time travels in `EBP`, the wrapper packs the fourth word and the time in a small array and loads them like the C system call wrapper does, so it is safe with `-fno-omit-frame-pointer`.

How it works (`cock/core/task/sync_ipc.*`):

* A caller is queued on the target thread (first come, first served) and set to `WAITING_REPLY`. A server with nothing queued is set to `WAITING_CALL`. Each thread keeps the call it serves (`serving`); only that thread can reply to it, so a reply cannot be forged and a call cannot be answered twice.
* **Direct handoff.** When a call finds the server in `WAITING_CALL`, the request is written into the server registers and the scheduler runs the server next, without going through the ready queues or looking at priorities (`Scheduler::requestHandoff`). `reply_wait` does the same toward the caller when nobody else is queued. A round trip is two system calls and two switches.
* **Deferred replies.** A server that cannot answer yet (a reader waiting for input) calls `SYS_IPC_DEFER`: the call it serves moves from `serving` to the `deferred` list of the server (the caller stays in `WAITING_REPLY`) and the server can `accept` the next call. Later `SYS_IPC_REPLY_TO` takes the ticket and answers it. The ticket is looked up only in the list of the calling thread, so a thread cannot answer a call that was not sent to it. A deferred caller is still covered by its timeout (`expire` removes it from the list) and by the death of the server (`abandon` wakes it with -1).
* The call fails at once (-1) when the target does not exist, is dead, or is the caller itself. When the **server dies** (an `exit` or an exception) every caller queued on it and the one being served wake up with -1.
* **One wait point for everything.** A thread in `WAITING_CALL` is also woken by an asynchronous message or an interrupt notification (`IPC::deliver` and `IPC::notify` call `SyncIpc::wakeForMessage`), and `accept` returns at once with `2` if one is already pending. Calls come first. A driver can therefore serve calls and handle its interrupt in one loop: when `accept` returns `2`, it empties the inbox with `SYS_IPC_TRY_RECV`.
* **Timeouts.** A time arms the thread with `Timeouts` and, when it runs out, `SyncIpc::expire` cleans up where the thread was: a caller still queued leaves the queue of the server; a caller being served is detached (`serving` is cleared), so the late answer of the server fails with `-1` instead of reaching a thread that has moved on; a waiting server just returns `1`. Anything that ends the wait first (an answer, a message, the death of the server) cancels the timeout. A call with time `0` is not sent at all.

`test/userland/syncbench` measures 2000 round trips between two threads of one process, with the asynchronous path (`ipc_send` and `ipc_recv` on both sides) and with calls. In QEMU it printed about 37000 cycles per round trip for the asynchronous path and about 10000 for the synchronous one (QEMU timing is not hardware timing; the test only requires the synchronous path to be faster).

## Interrupts as messages
A driver in userland handles a hardware interrupt like this:

1. it calls `SYS_IRQ_BIND` with the line (it needs an `IRQ` capability) and the kernel unmasks the line,
2. when the line fires, the kernel **masks it** and sets the bit of that line in the **pending notifications** of the owner thread, waking it if it was waiting,
3. the driver receives the notification as an `IRQ_NOTIFICATION` message from `SYS_IPC_RECV`, reads its device, then calls `SYS_IRQ_ACK`, and the kernel unmasks the line again.

A notification is a bit in the thread, one bit per line, not a message in the inbox. That has two consequences:

* it **cannot be lost**: a full inbox, full of messages from other programs, does not stop an interrupt from reaching its driver,
* it **cannot be forged**: a program is not allowed to send an `IRQ_NOTIFICATION`, and the one the driver receives carries `KERNEL_SENDER`.

Notifications of different lines are delivered lowest line first.

If the owner no longer exists when its line fires, the kernel releases the line instead of leaving it masked and owned.

The timer line and the PIC cascade line are reserved and cannot be bound. When the owner dies, the kernel masks and releases its lines. The keyboard driver in `userland/kbd` is the reference.

## Output from programs
`SYS_WRITE` is the debug and fallback output channel: programs that link `libc/unistd/console.cpp` send descriptors 1 and 2 to the console server instead (see [console](../userland/console.md)) and fall back to this call when the server is not there. The kernel writes the bytes to the serial port with interrupts disabled, so a call is limited to 128 bytes to keep that time short. It returns how many bytes it took, and `cock::user::write_text` calls again until everything is written. When the display server is alive the text also goes to it, in messages of 20 bytes.

## Logging of denied calls
A call that is denied (no capability, bad pointer, unknown number) is logged at `trace` level only, so a program that keeps asking cannot flood the serial log. Raise the log level in `cock_main` to see them.
