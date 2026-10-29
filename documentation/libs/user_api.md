# The `cock::user` API

`userland/lib/include/cock/user/api.hpp` wraps the system calls for the programs written in C++:

| Function | System call |
|----------|-------------|
| `ipc_send(pid, message)`, `ipc_recv(message)` | `SYS_IPC_SEND` (to a thread of the caller's own process; another program is reached with `ep_send`), `SYS_IPC_RECV`; `ipc_recv` returns the messages stashed by the C library first |
| `request_ports(first, count)` | `SYS_REQUEST_RESOURCE` |
| `irq_bind(line)`, `irq_ack(line)` | `SYS_IRQ_BIND`, `SYS_IRQ_ACK` |
| `cap_grant(pid, type, first, count, delegate)` | `SYS_CAP_GRANT`; `delegate` lets the receiver pass the capability on |
| `names::connect(name)`, `names::server(name)` (`names.hpp`) | A synchronous call to the name server: it checks the `ipc=` policy, gives the caller an `IPC` capability for the server and answers with its process id; `connect` returns the status and the id, `server` only the id (`NO_PROCESS` when refused) |
| `thread_process(tid)` | `SYS_THREAD_PROCESS`; the process a thread belongs to |
| `module_process(name)` | `SYS_MODULE_PROCESS`; the process of a boot module, for the root server |
| `shm_create(pages)`, `shm_share(id, pid, writable)`, `shm_map(id, writable)`, `shm_release(id)` | `SYS_SHM_CREATE`, `SYS_SHM_SHARE`, `SYS_SHM_MAP`, `SYS_SHM_RELEASE` |
| `port_out8(port, value)`, `port_in8(port)`, `port_read_words(port, buffer, words)` | Port instructions (`outb`, `inb`, `rep insw`), allowed after `request_ports` |
| `module_open(name)` | `SYS_MODULE_OPEN` |
| `module_index(name)` | `SYS_MODULE_INDEX` |
| `spawn(image, size, name, notify_exit, rights)` (`spawn.hpp`) | The loader in user space: reads the ELF image and builds the process with `SYS_PROC_CREATE`, `SYS_PROC_MAP`, `SYS_PROC_WRITE`, `SYS_PROC_PROTECT` and `SYS_PROC_START`, and destroys it with `SYS_PROC_DISCARD` if a step fails; `notify_exit` asks for a `PROCESS_EXITED` message when the process ends; `rights` (optional) gives the new process its service label, `IPC` capabilities for labels and one initial handle (whose number the program reads with `cock::user::initialHandle()`) before it runs |
| `service_set(pid, label)` | `SYS_SERVICE_SET`; labels a running process once (used for boot modules) |
| `cap_accept(mask)`, `cap_send(pid, type, first, count, delegate, tag)` | `SYS_CAP_ACCEPT`, and `SYS_IPC_SEND` of a `CAPABILITY_TRANSFER` message (returns the node id of the copy, `abi::NO_CAPABILITY_NODE` on failure) |
| `cap_grant_node(pid, type, first, count)`, `cap_revoke(node)` | `SYS_CAP_GRANT` returning the derivation node, and `SYS_CAP_REVOKE` |
| `process_alive(pid)` | `SYS_PROC_ALIVE` |
| `process_state_mask(pid)` | `SYS_PROC_STATE` |
| `proc_signal(pid, signal)` | `SYS_PROC_SIGNAL`; only `pm` holds the capability. Programs use `pm::Client::kill` |
| `proc_exec(handle, entry, pid, initial)` | `SYS_PROC_EXEC`; only `pm` holds the capability |
| `proc_fork(pid, reply)` | `SYS_PROC_FORK`; only `pm` holds the capability. Programs use `pm::Client::fork` |
| `exec(process, files, path, argv, argc)` (`exec.hpp`) | Replaces the image of the program with a file: reads it through the file server into an object of `pm` and sends `EXEC`; returns only when it failed (`ExecFailure`) |
| `argumentCount()`, `argument(index)` (`crt0.hpp`) | The arguments of a program that `exec` started, from the page at `abi::exec::ARGS_ADDRESS`; none for a program that `spawn` started. `_start` also passes them to `main`: a program may define `extern "C" int main(int argc, char **argv)` (`argv[argc]` is null) or `main()`, which ignores them |
| `ipc_try_recv(message)` | `SYS_IPC_TRY_RECV`; `Received::MESSAGE`, `EMPTY` or `FAILED` |
| `ipc_recv_timeout(message, milliseconds)` | `SYS_IPC_RECV_TIMEOUT`; the same results, `EMPTY` when the time ran out; like `ipc_try_recv` it reads only the kernel inbox |
| `sleep_ms(milliseconds)` | `SYS_SLEEP`; true when it slept (resolution 10 ms) |
| `thread_start(function, argument, stack, size)` (`thread.hpp`), `thread_create_raw`, `gettid()` | `SYS_THREAD_CREATE` and `SYS_GETTID`; the program provides the stack; the thread function is `void f(void *)` and may return |
| `ipc_call(target, words)` (a thread of the caller's own process), `ep_call(handle, words)` (another program), `ipc_accept()`, `ipc_reply(words)`, `ipc_reply_wait(words)` (`call.hpp`) | Synchronous calls in registers, four words each way, an optional time in milliseconds, `CallStatus` `OK`, `TIMED_OUT`, `MESSAGE_PENDING` or `FAILED`; see [ipc](../core/ipc.md#synchronous-calls) |
| `futex_wait(word, expected, ms)`, `futex_wake(word, count)`, `Mutex` (`mutex.hpp`), `ConditionVariable` (`condition_variable.hpp`), `Semaphore` (`semaphore.hpp`), `JoinableThread` with `join` (`joinable_thread.hpp`) | `SYS_FUTEX_WAIT` and `SYS_FUTEX_WAKE`; the last three are built on them and on `process_alive`, with no kernel change; see [threads](../core/threads/README.md) |
| `Clock` (`clock.hpp`), `init()`, `now(time)` | Asks the `rtc` server for the date and time (a program needs no port capability; `onForeign` receives the messages that arrive while it waits); `abi/datetime.hpp` converts it to Unix seconds and to FAT date and time. `Cmos` (`cmos.hpp`) is the reader of the ports `0x70` and `0x71` that only `rtc` uses |
| `self()` | `SYS_GETPID` |
| `write_text(text, length)` | `SYS_WRITE`, repeated until the whole text is written (a call takes at most 128 bytes) |
| `exit_process(status)` | `SYS_EXIT` |
| `port_in8(port)` | The `in` instruction (the process needs the port enabled) |

`mmap` and `munmap` are in the C library (`<sys/mman.h>`).
