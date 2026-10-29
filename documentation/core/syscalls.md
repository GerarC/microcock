# System calls

Every call the kernel serves, with what it is for, the arguments it takes, what it returns and what authority it needs. The handlers are declared in `cock/core/syscall/syscalls.hpp` (the source of truth for the details of each failure), the numbers in `cock/arch/x86/syscall/syscall_nums.hpp`, and the table that joins them in `cock/arch/x86/syscall/syscall_manager.cpp`. Programs call them through the wrappers of `cock::user` (`userland/lib/include/cock/user/api.hpp`, `call.hpp`) and of the C library (`libc/include/sys/syscalls.h`).

## How a call is made

| Item | Value |
|------|-------|
| Entry | Software interrupt `int 0x80` from Ring 3 |
| Number | `EAX` |
| Arguments | Up to six, in `EBX`, `ECX`, `EDX`, `ESI`, `EDI` and `EBP`, in the order of the tables below (argument 1 is `EBX`, argument 6 is `EBP`) |
| Result | `EAX` |
| Failure | All ones (`-1` as an unsigned value) for every call, unless the table says otherwise |
| Unused arguments | Ignored |
| Trust | Every argument is untrusted: a pointer is checked against the user range of the caller, a number is looked up before it is used, and an id carries a generation |

The numbers come in two families that never overlap. The Linux i386 numbers (0 to 439) are kept for the calls that have the name and the purpose of a Linux call: `exit` (1), `read` (3), `write` (4), `getpid` (20), `mmap` (90), `munmap` (91), `sched_yield` (158) and `gettid` (224). Every other Linux number fails with all ones and is reserved for a Linux personality. The calls of the microkernel start at `0x200`, in blocks of 16 by purpose: processes and threads (`0x200`), time and synchronization (`0x210`), asynchronous messages (`0x220`), synchronous calls (`0x230`), capabilities (`0x240`), hardware (`0x250`), shared memory (`0x260`) and boot modules (`0x270`). A number with no name, and a number past the table, also fail. See [IPC](ipc.md#numbering).

The words "needs" in the tables name a capability (see [capabilities](capabilities.md)): `SPAWN` is the right to manage processes, *grant authority* is the right to hand out capabilities, and the others (`IO_PORTS`, `IRQ`, `PHYSICAL_MEMORY`, `BOOT_MODULE`) cover a range of hardware or of boot modules.

## Process and thread

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_EXIT` | `0x001` | Ends the calling thread and tells the task manager to clean up; the process ends with its last thread. | 1 `status`: exit status, truncated to 32 bits | 0; the caller never runs again | |
| `SYS_GETPID` | `0x014` | Gives the process id of the caller. | none | The process id; all ones when there is no current thread | |
| `SYS_GETTID` | `0x0E0` | Gives the id of the calling thread. | none | The thread id; all ones when there is no current thread | |
| `SYS_THREAD_CREATE` | `0x207` | Starts another thread of the caller's process on a stack the caller owns. | 1 `entry`: first instruction; 2 `stack_base`: lowest address of the stack, writable user memory; 3 `stack_size`: bytes, at least `MIN_THREAD_STACK_BYTES` (8 KiB) | The new thread id; all ones when the caller is a kernel thread, the stack is too small, entry or stack are invalid, or no thread can be made | |
| `SYS_THREAD_PROCESS` | `0x208` | Gives the process a thread belongs to; a server that serves a synchronous call uses it to know whose the call is. | 1 `tid`: a thread id (a process id names its first thread) | The process id; all ones when the id is unknown, the thread is dead or it belongs to the kernel | |
| `SYS_PROC_ALIVE` | `0x206` | Tells whether a process id is alive. | 1 `pid`: process id, generation included | 0 when the thread exists and is not dead; all ones otherwise | |
| `SYS_PROC_STATE` | `0x209` | Tells in which states the live threads of a process are; `pm` turns it into the `R` or `S` of `ps`. Read only. | 1 `pid`: process id, generation included | A mask of `THREAD_READY` (0x01), `THREAD_RUNNING` (0x02), `THREAD_BLOCKED` (0x04), `THREAD_WAITING_MSG` (0x08), `THREAD_WAITING_CALL` (0x10), `THREAD_WAITING_REPLY` (0x20) and `THREAD_STOPPED` (0x40), one bit for each state a live thread is in; all ones when the id is not a process, is the kernel or has no live thread | |
| `SYS_PROC_SIGNAL` | `0x20C` | Sends a signal to another process. `SIGNAL_KILL` (9) ends every thread, exit status 137; `SIGNAL_TERMINATE` (15) and `SIGNAL_INTERRUPT` (2) queue a `SIGNAL` message in the inbox of the main thread; `SIGNAL_STOP` (19) keeps its threads from running; `SIGNAL_CONTINUE` (18) lets them run. Stop and continue succeed when the process is already in that state. Who may signal whom is the policy of `pm`. | 1 `pid`: process id, generation included; 2 `signal`: one of the five numbers | 0; all ones when the caller has no `SPAWN`, the id is not the main thread of a live user process, the target is the caller, the signal is unknown or the inbox is full | `SPAWN` |

### Starting programs from user space

A program is started with six calls, which `cock::user::spawn` makes after it has read the ELF file; the kernel does not parse the file. A process made this way is *pending* until it starts: it has an address space and no thread, only its creator may use its handle, and it dies with its creator.

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_PROC_CREATE` | `0x200` | Creates a pending process with an empty address space. | 1 `name_ptr`: the process name; 2 `name_length`: from 1 to the name limit minus one; 3 `flags`: `SPAWN_NOTIFY_EXIT` (1) makes the kernel send the creator a message when the process exits; 4 `rights_ptr`: zero, or an `abi::service::SpawnRights` (the label of the process, the IPC labels it may talk to, and one initial handle on an endpoint); 5 `user_limit`: first address the process may not use, page aligned, not above the architecture limit and above the user stack | The handle of the pending process; all ones when the caller has no `SPAWN`, it has too many pending processes (2 each, 8 in the system), a number or buffer is invalid, no process id is free, the rights are invalid (each IPC label needs a grantable IPC capability of the caller) or memory runs out | `SPAWN` |
| `SYS_PROC_MAP` | `0x201` | Maps zeroed, writable pages into a pending process. | 1 `handle`; 2 `address`: page aligned, not in the first page; 3 `pages`: not zero. The range must end below the user stack, be unmapped and keep the process under 4 MiB | 0; all ones for a handle that is not the caller's, a bad range, an overlap or no memory (a failure in the middle can leave pages mapped; the caller discards the process) | |
| `SYS_PROC_WRITE` | `0x202` | Copies bytes from the caller into mapped, writable pages of a pending process. | 1 `handle`; 2 `address`: destination in the pending process; 3 `source`: in the caller, readable; 4 `length`: not zero, at most 4 MiB | 0; all ones for a bad handle, an unreadable source or a destination that is not mapped and writable | |
| `SYS_PROC_PROTECT` | `0x203` | Makes mapped pages of a pending process read only, so the program cannot change its code. | 1 `handle`; 2 `address`: page aligned; 3 `pages`: not zero, every page mapped | 0; all ones for a bad handle or range | |
| `SYS_PROC_START` | `0x204` | Creates the first thread of a pending process at an entry point, sets its rights and lets it run. The handle stops being valid. | 1 `handle`; 2 `entry`: inside a mapped page below the user stack | The id of the first thread, which is the process id; all ones when the handle is not the caller's, the entry is invalid, no id is free, the rights are no longer valid or the caller has too many children with an exit notice (the process stays pending) | |
| `SYS_PROC_DISCARD` | `0x205` | Destroys a pending process and frees its memory. | 1 `handle` | 0; all ones for a handle that is not the caller's | |
| `SYS_PROC_EXEC` | `0x20A` | Replaces the program image of a process with a pending one and keeps its id, capabilities, endpoints, service label and parent; its other threads end. The target must be waiting for the reply of a call the caller serves, which is its consent. | 1 `handle`: a pending process made with no rights and no exit notice; 2 `entry_point`; 3 `pid`: the target, the main thread of a user process other than the caller's; 4 `initial`: the number the new image receives in `EAX` | 0 and the handle stops being valid; all ones with the old image untouched | `SPAWN` |
| `SYS_PROC_FORK` | `0x20B` | Makes a copy of a process: a child of the caller whose memory is shared copy on write, with one thread that resumes inside the call the target made, and the `IPC` and `ENDPOINT` capabilities of the target. Hardware capabilities, `SPAWN` and the owner of an endpoint are not copied. | 1 `pid`: the target, waiting for the reply of a call the caller serves; 2 `reply_ptr`: four 32 bit words, the answer the new thread finds | The id of the new process; all ones with nothing created | `SPAWN` |

## Memory

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_MMAP` | `0x05A` | Maps pages into the caller. With `MAP_ANONYMOUS` (0x20) the pages are zero filled, private and read and write; with `MAP_PHYS` (0x1000) it maps device memory at a fixed address, and the range must not be RAM. | 1 `addr`: fixed address, used only with `MAP_PHYS`; 2 `length`: bytes, rounded up to pages, not zero; 3 `prot`: ignored; 4 `flags`; 5 `fd`: ignored; 6 `offset`: the physical address for `MAP_PHYS` | The start of the mapping; all ones when no flag is given, the size is zero, the quota or the region slots run out, no range is free, memory runs out, or the address is outside the user image range | `PHYSICAL_MEMORY` for `MAP_PHYS` |
| `SYS_MUNMAP` | `0x05B` | Frees pages obtained with `SYS_MMAP` or `SYS_SHM_MAP`. A shared mapping also drops the mapping count of its object. | 1 `addr`: page aligned start of a region; 2 `length`: bytes, rounded up to pages | 0; all ones when the arguments do not describe a region of the caller | |

## Time and synchronization

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_SCHED_YIELD` | `0x09E` | Gives the CPU to the next ready thread. | none | 0 | |
| `SYS_SLEEP` | `0x210` | Blocks the caller for at least the given time (10 ms resolution). | 1 `milliseconds`: at most `0x7FFFFFFF`; zero only yields | 0; all ones when the time is too large | |
| `SYS_FUTEX_WAIT` | `0x211` | Blocks while a 32 bit word still holds an expected value. | 1 `address`: an aligned word in user memory; 2 `expected`; 3 `milliseconds`: a finite time, or `FUTEX_NO_TIMEOUT` (`0xFFFFFFFF`) to wait for ever | `FUTEX_WOKEN` (0), `FUTEX_VALUE_CHANGED` (1) when the word did not hold the value, `FUTEX_TIMED_OUT` (2); all ones when the address is null, unaligned or not readable, or the time is invalid | |
| `SYS_FUTEX_WAKE` | `0x212` | Wakes threads of the caller's process that wait on a word. | 1 `address`: the word; 2 `count`: most threads to wake (`FUTEX_WAKE_ONE` 1, `FUTEX_WAKE_ALL` `0xFFFFFFFF`) | The number of threads woken; all ones for an invalid address | |
| `SYS_SCHED_METRICS` | `0x213` | Writes the response, CPU and turnaround ticks of the calling thread. Registered only in a kernel built with `SCHED_METRICS=1`; otherwise it fails like any unknown call. | 1 `buffer`: an `abi::sched::Metrics` in writable user memory | 0; all ones when the buffer is not writable or the call is not registered | |

## Asynchronous messages

A message is a `Message` (`shared/include/cock/ipc/message.hpp`) copied by the kernel. The kernel clears the `badge` and `reply` fields of a message a program sends and writes its own. A thread of another process is never named by id: it is reached through a handle on an endpoint (`SYS_EP_SEND`).

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_IPC_SEND` | `0x220` | Sends a message to a thread of the calling process. | 1 `to_pid`: destination thread id (for a first thread, the process id); 2 `msg_ptr`: a `Message`, copied before use | 0 when queued; all ones when the buffer is invalid, the type is reserved for the kernel (interrupt notice, exit notice), the target is unknown, dead, of another process or has a full inbox. A `CAPABILITY_TRANSFER` message always fails here, because the receiver is the sender | Grant authority for a capability grant message |
| `SYS_IPC_RECV` | `0x221` | Takes the next notification or message of the thread and waits when there is none. An interrupt notice comes before queued messages. | 1 `msg_ptr`: where the `Message` is stored, writable | 0; all ones for an invalid buffer | |
| `SYS_IPC_TRY_RECV` | `0x222` | Like `SYS_IPC_RECV` without waiting. | 1 `msg_ptr` | 0 when a message was stored; 1 when none is pending; all ones for an invalid buffer | |
| `SYS_IPC_RECV_TIMEOUT` | `0x223` | Like `SYS_IPC_RECV`, giving up after a time. | 1 `msg_ptr`; 2 `milliseconds`: at most `0x7FFFFFFF` | 0 when a message was stored; 1 on timeout; all ones for an invalid buffer or time | |
| `SYS_EP_SEND` | `0x224` | Sends a message through a handle on an endpoint. The kernel writes the badge of the handle in `Message::badge`. A `CAPABILITY_TRANSFER` message gives the receiver a copy of part of a capability of the sender. | 1 `handle`: an `ENDPOINT` capability of the caller; 2 `msg_ptr`; 3 `flags`: zero, or `SEND_EXPECT_REPLY` to open a reply token the receiver finds in `Message::reply` | As `SYS_IPC_SEND`; all ones also when the handle names no endpoint capability of the caller, the endpoint is unbound or the process bound to it is gone, or a token was asked for and the table is full. A capability message returns the node id of the copy | An `ENDPOINT` capability |
| `SYS_EP_REPLY` | `0x225` | Answers a message that carried a reply token, once. | 1 `token`: from `Message::reply`, held by the caller's process; 2 `msg_ptr`: reserved types and `CAPABILITY_TRANSFER` are refused | 0 when queued for the thread that asked and the token was spent; all ones when the pointer or type is bad or the caller does not hold the token (a gone asker closes the token; a full inbox leaves it open) | |
| `SYS_EP_FORWARD` | `0x226` | Passes a message and its token to another endpoint, so that the process bound to it answers. | 1 `token`; 2 `handle`: the caller's handle on the endpoint that should answer; 3 `msg_ptr` | 0 when queued; all ones when the caller does not hold the token, the handle leads nowhere or the message does not fit (the caller keeps the token) | An `ENDPOINT` capability |

## Synchronous calls

A call carries four 32 bit words each way, in registers, with a direct switch to the server. The status comes in `EAX`: `SUCCESS` (0), `TIMED_OUT` (1), `MESSAGE_PENDING` (2, an asynchronous message arrived and is read with `SYS_IPC_TRY_RECV`) or `FAILURE` (all ones). On success the peer comes back in `EBX` and the four words in `ECX`, `EDX`, `ESI` and `EDI`. The time is `NO_TIMEOUT` (`0xFFFFFFFF`) to wait for ever, or at most `0x7FFFFFFF` milliseconds; zero never waits.

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_IPC_CALL` | `0x230` | Sends four words to a thread of the calling process and blocks until it replies. | 1 `target`: thread id of the server, of the caller's process; 2 to 5 `word0` to `word3`; 6 `milliseconds` | The status; on success the four words of the reply | |
| `SYS_IPC_ACCEPT` | `0x231` | Waits for the next call. It also wakes for an asynchronous message or an interrupt. | 6 `milliseconds` | The status; on success the caller id in `EBX` and the four words of the request | |
| `SYS_IPC_REPLY` | `0x232` | Answers the call being served. | 2 to 5 `word0` to `word3` | Success; failure when the thread is not serving a call | |
| `SYS_IPC_REPLY_WAIT` | `0x233` | Answers the call being served, if any, and waits for the next one. | 2 to 5 `word0` to `word3`; 6 `milliseconds` | As `SYS_IPC_ACCEPT`; failure when the reply is refused or the time is invalid | |
| `SYS_IPC_DEFER` | `0x234` | Detaches the call being served and gives a ticket, so the server can accept other calls and answer this one later. | none | The ticket (the thread id of the caller); failure when no call is being served | |
| `SYS_IPC_REPLY_TO` | `0x235` | Answers a detached call. | 1 `ticket`: from `SYS_IPC_DEFER`; 2 to 5 `word0` to `word3` | Success; failure when the ticket does not name a call detached by the caller | |
| `SYS_EP_CALL` | `0x236` | Calls the process bound to an endpoint. The server finds the badge of the handle as the peer of its accept. | 1 `handle`: an `ENDPOINT` capability of the caller; 2 to 5 `word0` to `word3`; 6 `milliseconds` | As `SYS_IPC_CALL` | An `ENDPOINT` capability |

## Capabilities and endpoints

See [capabilities](capabilities.md) and [endpoints](endpoints.md) for the model: a capability is a right over a range, recorded in a derivation tree, and a handle is the capability a program uses to reach an endpoint.

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_CAP_GRANT` | `0x240` | Gives part of a capability the caller holds to another process, recorded in the derivation tree. | 1 `target_pid`: not a kernel process; 2 `type`: a capability type; 3 `first`: start of the range (port, line, address, module index); 4 `count`: length; 5 `flags`: zero, `GRANT_DELEGATE` (the target may grant the copy on), `GRANT_MINT` (`ENDPOINT` only: the target may choose badges), or both; 6 `badge`: `ENDPOINT` only | The node id of the grant (above 0 and below all ones); all ones when the type or flags are invalid, the caller holds no grantable capability that covers the range, a mint right or a badge is asked for without the mint right, the target is invalid, or a table is full | A grantable capability that covers the range |
| `SYS_CAP_REVOKE` | `0x241` | Revokes a delegated capability and everything derived from it; closes the ports, unbinds the lines and unmaps the memory the holders opened with it. | 1 `node`: as `SYS_CAP_GRANT` returned it | 0; all ones when the id is zero, unknown, or the caller holds no ancestor of the node | An ancestor of the node |
| `SYS_CAP_ACCEPT` | `0x242` | Chooses which capability types a `CAPABILITY_TRANSFER` message may give the caller. A process accepts nothing until it calls this. | 1 `mask`: one bit per capability type | 0; all ones when the mask has a bit outside the known types | |
| `SYS_SERVICE_SET` | `0x243` | Gives a running process its service label, once. | 1 `target_pid`: a live user process; 2 `label`: not `NO_SERVICE` | 0; all ones when the target is invalid, is a thread id or a kernel process, or already has a label | Grant authority |
| `SYS_EP_CREATE` | `0x244` | Creates an endpoint and gives the caller the capability that owns it (grant, mint and bind rights). | 1 `handle_ptr`: a 32 bit value that receives the handle, or `NO_CAPABILITY_NODE` when the caller already held a capability with the bind right that covers the new id | The endpoint id (from 1); all ones when a table is full or the pointer is invalid; nothing is created then | Grant authority |
| `SYS_EP_BIND` | `0x245` | Binds an endpoint to the process that receives what is sent to it; a previous owner stops receiving. | 1 `endpoint`: the id; 2 `pid`: a live user process | 0; all ones otherwise | An `ENDPOINT` capability with the bind right that covers the id |

## Hardware

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_REQUEST_RESOURCE` | `0x250` | Enables I/O ports in the permission bitmap of the process. | 1 `resource_id`: the first port; 2 `size`: how many consecutive ports | 0; all ones when no capability covers the whole range, the range is empty or out of range, the ports belong to the kernel, or memory runs out | `IO_PORTS` |
| `SYS_IRQ_BIND` | `0x251` | Routes an interrupt line to the calling thread; each interrupt arrives as a notification. | 1 `line` | 0; all ones when the line is reserved or already owned | `IRQ` |
| `SYS_IRQ_ACK` | `0x252` | Unmasks a line the calling thread owns, after servicing the device. | 1 `line` | 0; all ones when the caller is not the owner | |

## Shared memory

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_SHM_CREATE` | `0x260` | Creates a shared memory object owned by the caller, paid by its page quota. | 1 `pages`: from 1 to the object limit | The object id; all ones when the size is invalid, the quota is over, the table is full or frames run out | |
| `SYS_SHM_SHARE` | `0x261` | Gives another process access to an object. | 1 `id`: held by the caller; 2 `target_pid`; 3 `writable`: non zero for write access, which the caller must have | 0; all ones on any failure | |
| `SYS_SHM_MAP` | `0x262` | Maps a whole object into the caller. | 1 `id`; 2 `writable`: non zero for a writable mapping | The address of the mapping; all ones when access is denied, no region slot or range is free or a page cannot be mapped | |
| `SYS_SHM_RELEASE` | `0x263` | Drops the right of the caller to an object; existing mappings stay until they are unmapped. | 1 `id` | 0 when the caller was a holder; all ones otherwise | |

## Boot modules

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_MODULE_PROCESS` | `0x270` | Finds the process the kernel started from a boot module, by module name. Programs find a server through the name server instead. | 1 `name_ptr`: the name, not terminated; 2 `length`: from 1 up to the name limit minus one | The process id; all ones when the arguments are invalid or no boot module process has the name | Grant authority |
| `SYS_MODULE_OPEN` | `0x271` | Opens a data boot module by name as a read only shared memory object. | 1 `name_ptr`; 2 `length` | The object id; all ones when the name is unknown, the buffer is invalid or the caller holds no capability for that module | `BOOT_MODULE` |
| `SYS_MODULE_INDEX` | `0x272` | Gives the index of a boot module in the boot list; the index names the capability to grant. | 1 `name_ptr`; 2 `length` | The index; all ones otherwise | Grant authority |

## Debug input and output

The kernel has no input queue and no terminal: these two calls exist for the processes that started before the console server, and the C library uses them only when the console client is missing or fails. Programs read and write through the [console server](../userland/drivers/console.md).

| Call | Number | What it does | Arguments | Returns | Needs |
|------|--------|--------------|-----------|---------|-------|
| `SYS_WRITE` | `0x004` | Writes text to the console: to the display server, or to the kernel console when there is none or it died. | 1 `fd`: 1 or 2, both go to the same console; 2 `buf_ptr`: the bytes; 3 `count`: at most 128 per call | The bytes accepted, which may be fewer than `count`; all ones for a bad descriptor or buffer; 0 with a restart when the display server's inbox is full (after 1000 restarts the text goes to the kernel console) | |
| `SYS_READ` | `0x003` | Validates the buffer and reports the end of input. | 1 `fd`: only 0; 2 `buf`: writable; 3 `count` | 0, since the kernel has no input; all ones for another descriptor or an invalid buffer | |

## Numbers that fail

Every other Linux i386 number, every number without a name and every number past the table reaches `sys_not_implemented`, which touches nothing, logs the number at trace level and returns all ones.
