/**
 * @file api.hpp
 * @brief Thin C++ wrappers over the kernel system calls and the x86 port I/O instructions.
 *
 * Every wrapper issues one `syscall` and turns the raw result (`FAILURE` is `-1`) into a
 * bool, a sentinel id or a typed value. The kernel validates every pointer and number,
 * so a bad argument makes the call fail and nothing else happens. Also keeps the small
 * per-process stash of messages that were received while waiting for something else.
 * The stash is not thread safe: use it from one thread only.
 */
#ifndef COCK_USER_API_HPP
#define COCK_USER_API_HPP

#include <cock/abi/capability.hpp>
#include <cock/abi/process.hpp>
#include <cock/abi/sched_metrics.hpp>
#include <cock/ipc/message.hpp>
#include <stddef.h>
#include <stdint.h>
#include <sys/syscalls.h>

namespace cock::user {

/** Raw system call result that means failure (`-1`). */
constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
/** Process or thread id returned by wrappers when the operation failed or the name is unknown. */
constexpr uint32_t NO_PROCESS = 0xFFFFFFFF;
/** Shared memory object id returned when creation or opening failed. */
constexpr uint32_t NO_SHM = 0xFFFFFFFF;
/** Boot module index returned when the module does not exist or the call was refused. */
constexpr uint32_t NO_MODULE = 0xFFFFFFFF;
/** Raw system call result that means success (`0`). */
constexpr uintptr_t SUCCESS = 0;
/** Raw result of the non blocking and timed receive calls when no message arrived. */
constexpr uintptr_t NO_MESSAGE = 1;
/** File descriptor for console output, accepted by `SYS_WRITE` together with descriptor 2. */
constexpr int STDOUT_FILE = 1;

/**
 * @brief Returns the id of the calling process (`SYS_GETPID`).
 * @return The process id. The call cannot fail.
 */
inline uint32_t self() {
	return static_cast<uint32_t>(syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0));
}

/**
 * @brief Sends an asynchronous message (`SYS_IPC_SEND`). Never blocks.
 *
 * The inbox belongs to a thread, not to the process: a process id is the id of its first
 * thread, so the message reaches that thread, and only that thread receives it.
 * @param pid Id of the target thread (for a first thread, the process id).
 * @param message Message to copy; the kernel stamps `sender_pid` with the id of the
 *        sending thread and `target_pid` with the id of the receiving thread.
 * @return true when the message was queued; false when the target does not exist or is
 *         dead, its inbox (16 messages) is full, the pointer is bad or the message type is
 *         not allowed for this sender (`IRQ_NOTIFICATION` and `PROCESS_EXITED` never;
 *         `CAPABILITY_GRANTED` only with grant authority). Nothing is retried.
 */
inline bool ipc_send(uint32_t pid, const ipc::Message &message) {
	return syscall(SYS_IPC_SEND, pid, reinterpret_cast<uintptr_t>(&message), 0,
				   0, 0, 0) == SUCCESS;
}

/** Number of messages the per-process stash can hold. */
constexpr size_t STASH_CAPACITY = 16;
/** Index of the oldest stashed message. */
constexpr size_t FIRST_STASHED = 0;

/** Messages that arrived while a reply was awaited, in arrival order. Not thread safe. */
inline ipc::Message stashedMessages[STASH_CAPACITY];
/** Number of valid entries in `stashedMessages`. */
inline size_t stashedCount = 0;

/**
 * @brief Keeps a message so that a later `ipc_recv` returns it first.
 * @param message Message to copy into the stash.
 * @note When the stash is full (`STASH_CAPACITY`) the message is dropped silently.
 */
inline void stash_message(const ipc::Message &message) {
	if (stashedCount < STASH_CAPACITY) stashedMessages[stashedCount++] = message;
}

/**
 * @brief Removes the oldest stashed message.
 * @param message Receives the message when one was stashed.
 * @return true when a message was taken; false when the stash is empty.
 */
inline bool take_stashed(ipc::Message &message) {
	if (stashedCount == 0) return false;
	message = stashedMessages[FIRST_STASHED];
	stashedCount--;
	for (size_t index = 0; index < stashedCount; index++)
		stashedMessages[index] = stashedMessages[index + 1];
	return true;
}

/**
 * @brief Receives the next message (`SYS_IPC_RECV`).
 *
 * Returns a stashed message first. Otherwise blocks until a pending interrupt
 * notification or a message in the inbox of the calling thread arrives. Messages sent to
 * a process id land in the inbox of its first thread only.
 * @param message Receives the message.
 * @return true when a message was copied; false when the kernel refused the call
 *         (bad pointer).
 */
inline bool ipc_recv(ipc::Message &message) {
	if (take_stashed(message)) return true;
	return syscall(SYS_IPC_RECV, reinterpret_cast<uintptr_t>(&message), 0, 0, 0,
				   0, 0) == SUCCESS;
}

/** Outcome of a receive that can come back without a message. */
enum class Received : uint32_t {
	/** A message was copied into the buffer. */
	MESSAGE,
	/** No message arrived (nothing pending, or the time ran out). */
	EMPTY,
	/** The kernel refused the call (bad pointer or time above the limit). */
	FAILED,
};

/**
 * @brief Receives the next message without blocking (`SYS_IPC_TRY_RECV`).
 *
 * Does not look at the stash. Takes a pending interrupt notification first, then the inbox.
 * @param message Receives the message when one is pending.
 * @return `MESSAGE`, `EMPTY` when nothing is pending, or `FAILED` on a bad pointer.
 */
inline Received ipc_try_recv(ipc::Message &message) {
	uintptr_t result = syscall(SYS_IPC_TRY_RECV, reinterpret_cast<uintptr_t>(&message),
							   0, 0, 0, 0, 0);
	if (result == SUCCESS) return Received::MESSAGE;
	return result == NO_MESSAGE ? Received::EMPTY : Received::FAILED;
}

/**
 * @brief Receives the next message waiting at most a given time (`SYS_IPC_RECV_TIMEOUT`).
 *
 * Does not look at the stash. Blocks the thread until a message arrives or the time
 * runs out. The clock ticks every 10 ms and the time is rounded up to whole ticks, plus
 * a small safety margin, so the wait is never shorter than asked.
 * @param message Receives the message when one arrives.
 * @param milliseconds Longest wait; `0` never waits (it behaves like `ipc_try_recv`).
 *        Values above `0x7FFFFFFF` fail.
 * @return `MESSAGE`, `EMPTY` when the time ran out, or `FAILED` on a bad pointer or time.
 */
inline Received ipc_recv_timeout(ipc::Message &message, uint32_t milliseconds) {
	uintptr_t result = syscall(SYS_IPC_RECV_TIMEOUT,
							   reinterpret_cast<uintptr_t>(&message), milliseconds,
							   0, 0, 0, 0);
	if (result == SUCCESS) return Received::MESSAGE;
	return result == NO_MESSAGE ? Received::EMPTY : Received::FAILED;
}

/**
 * @brief Blocks the calling thread for a time (`SYS_SLEEP`).
 *
 * Messages that arrive meanwhile wait in the inbox and do not wake the thread.
 * @param milliseconds Time to sleep, rounded up to whole 10 ms ticks plus a small safety
 *        margin, so never shorter than asked; `0` only yields the CPU and returns true.
 *        Values above `0x7FFFFFFF` fail.
 * @return true when the sleep ended; false when the time was refused.
 */
inline bool sleep_ms(uint32_t milliseconds) {
	return syscall(SYS_SLEEP, milliseconds, 0, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Asks the kernel for direct access to an I/O port range (`SYS_REQUEST_RESOURCE`).
 *
 * Needs an `IO_PORTS` capability that covers the range. On success the ports are enabled
 * in the permission bitmap of the process and `port_in8` and friends work.
 * @param first First port number.
 * @param count Number of consecutive ports.
 * @return true when granted; false when the capability does not cover the range, the range
 *         is empty or beyond the port space, or it overlaps ports that belong to the kernel
 *         (PIC `0x20`-`0x21`, `0xA0`-`0xA1` and timer `0x40`-`0x43`) or no memory is left.
 */
inline bool request_ports(uint32_t first, uint32_t count) {
	return syscall(SYS_REQUEST_RESOURCE, first, count, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Binds an interrupt line to the calling process (`SYS_IRQ_BIND`).
 *
 * Needs an `IRQ` capability that covers the line. The kernel unmasks the line and, for
 * each interrupt, queues an `IRQ_NOTIFICATION` message for the calling thread (the owner is
 * the thread, not the process); it keeps the line masked until `irq_ack` is called.
 * @param line Interrupt line number.
 * @return true when bound; false when the capability is missing, the line is reserved
 *         (timer, cascade or out of range) or already has an owner.
 */
inline bool irq_bind(uint32_t line) {
	return syscall(SYS_IRQ_BIND, line, 0, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Acknowledges an interrupt so the kernel unmasks the line again (`SYS_IRQ_ACK`).
 * @param line Interrupt line previously bound by the calling thread.
 * @return true when acknowledged; false when the calling thread does not own the line
 *         (another thread of the same process does not count) or the line is reserved.
 */
inline bool irq_ack(uint32_t line) {
	return syscall(SYS_IRQ_ACK, line, 0, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Passes part of a capability held by the caller to another process (`SYS_CAP_GRANT`).
 *
 * The caller needs a capability of that type with the grant flag that covers the range.
 * @param pid Process that receives the capability; it must be a live user process.
 * @param type Kind of capability (ports, IRQ, boot module, ...).
 * @param first First port, line or module index.
 * @param count Number of items from `first`.
 * @return true when granted; false when the type is unknown, the caller holds no grantable
 *         capability covering the range, the target does not exist or is a kernel process,
 *         or the capability table of the target is full.
 */
inline bool cap_grant(uint32_t pid, abi::CapabilityType type, uintptr_t first,
					  uintptr_t count) {
	return syscall(SYS_CAP_GRANT, pid, static_cast<uintptr_t>(type), first,
				   count, 0, 0) == SUCCESS;
}

/**
 * @brief Finds a process by name (`SYS_PROC_LOOKUP`).
 * @param name NUL terminated process name; its length is measured here.
 * @return The process id, or `NO_PROCESS` when no live user process has that name or the
 *         name is empty or too long for the kernel.
 */
inline uint32_t lookup(const char *name) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result = syscall(SYS_PROC_LOOKUP, reinterpret_cast<uintptr_t>(name),
							   length, 0, 0, 0, 0);
	return result == FAILURE ? NO_PROCESS : static_cast<uint32_t>(result);
}

/**
 * @brief Creates a shared memory object (`SYS_SHM_CREATE`). The caller becomes its holder.
 * @param pages Size in pages.
 * @return The object id, or `NO_SHM` when the size is refused (zero or above the limit)
 *         or memory ran out.
 */
inline uint32_t shm_create(size_t pages) {
	uintptr_t result = syscall(SYS_SHM_CREATE, pages, 0, 0, 0, 0, 0);
	return result == FAILURE ? NO_SHM : static_cast<uint32_t>(result);
}

/**
 * @brief Makes another live process a holder of an object (`SYS_SHM_SHARE`).
 *
 * Only a holder can share and nobody can give more access than it has. Sharing again
 * with a holder only raises its access.
 * @param id Object id.
 * @param pid Process that receives access.
 * @param writable true to allow writing.
 * @return true when shared; false when the caller is not a holder, asks for more access
 *         than it has or the target is not alive.
 */
inline bool shm_share(uint32_t id, uint32_t pid, bool writable) {
	return syscall(SYS_SHM_SHARE, id, pid, writable ? 1 : 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Maps an object into the address space of the caller (`SYS_SHM_MAP`).
 * @param id Object id; the caller must be a holder.
 * @param writable true to map it writable (needs write access).
 * @return The address of the mapping, or nullptr when the caller is not a holder, lacks
 *         the access, the process has no free region slot or no address range is free.
 *         Unmap with `munmap` using the exact address and size of the object.
 */
inline void *shm_map(uint32_t id, bool writable) {
	uintptr_t result = syscall(SYS_SHM_MAP, id, writable ? 1 : 0, 0, 0, 0, 0);
	return result == FAILURE ? nullptr : reinterpret_cast<void *>(result);
}

/**
 * @brief Drops the hold of the caller on an object (`SYS_SHM_RELEASE`).
 * @param id Object id.
 * @return true when released; false when the caller is not a holder.
 */
inline bool shm_release(uint32_t id) {
	return syscall(SYS_SHM_RELEASE, id, 0, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Opens a data boot module as a read only shared object (`SYS_MODULE_OPEN`).
 *
 * Needs a `BOOT_MODULE` capability that covers the module. Map the result with
 * `shm_map(id, false)`.
 * @param name NUL terminated module name; its length is measured here.
 * @return The object id, or `NO_SHM` when the module is unknown, not covered by a
 *         capability of the caller, the name is empty or too long, or the pointer is bad.
 */
inline uint32_t module_open(const char *name) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result = syscall(SYS_MODULE_OPEN, reinterpret_cast<uintptr_t>(name),
							   length, 0, 0, 0, 0);
	return result == FAILURE ? NO_SHM : static_cast<uint32_t>(result);
}

/**
 * @brief Gives the index of a data boot module (`SYS_MODULE_INDEX`).
 *
 * Only a process with grant authority may call it; `init` uses it to grant the
 * `BOOT_MODULE` capability for one module.
 * @param name NUL terminated module name; its length is measured here.
 * @return The module index, or `NO_MODULE` when unknown or not allowed.
 */
inline uint32_t module_index(const char *name) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result = syscall(SYS_MODULE_INDEX, reinterpret_cast<uintptr_t>(name),
							   length, 0, 0, 0, 0);
	return result == FAILURE ? NO_MODULE : static_cast<uint32_t>(result);
}

/**
 * @brief Starts a process from an ELF image in memory (`SYS_SPAWN`).
 *
 * Only a process with grant authority may call it. The new process starts with an empty
 * capability set; the caller grants what it needs with `cap_grant`.
 * @param image ELF image in the memory of the caller (up to 4 MiB).
 * @param size Image size in bytes.
 * @param name NUL terminated process name; its length is measured here.
 * @param notify_exit true to receive a `PROCESS_EXITED` message when the process ends.
 * @return The new process id (the id of its first thread), or `NO_PROCESS` when the caller
 *         may not spawn, a size is zero or too large, the image is not a loadable ELF, ids
 *         or memory ran out, or a pointer is bad.
 */
inline uint32_t spawn(const void *image, size_t size, const char *name,
					  bool notify_exit = false) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result =
		syscall(SYS_SPAWN, reinterpret_cast<uintptr_t>(image), size,
				reinterpret_cast<uintptr_t>(name), length,
				notify_exit ? abi::process::SPAWN_NOTIFY_EXIT : 0, 0);
	return result == FAILURE ? NO_PROCESS : static_cast<uint32_t>(result);
}

/**
 * @brief Returns the id of the calling thread (`SYS_GETTID`).
 *
 * This is the id that synchronous calls (`ipc_call`) take as a target.
 * @return The thread id. The call cannot fail.
 */
inline uint32_t gettid() {
	return static_cast<uint32_t>(syscall(SYS_GETTID, 0, 0, 0, 0, 0, 0));
}

/**
 * @brief Reads the scheduler figures of the calling thread (`SYS_SCHED_METRICS`).
 * @param metrics Receives the response, CPU and turnaround time in timer ticks.
 * @return True on success. False when the kernel was built without `SCHED_METRICS=1`.
 */
inline bool sched_metrics(abi::sched::Metrics &metrics) {
	return syscall(SYS_SCHED_METRICS, reinterpret_cast<uintptr_t>(&metrics), 0, 0, 0, 0,
				   0) != FAILURE;
}

/**
 * @brief Starts a thread of the calling process (`SYS_THREAD_CREATE`), low level.
 *
 * The program owns the stack; the kernel never maps or frees it. Prefer `thread_start`,
 * which also builds the first stack frame.
 * @param entry Address where the thread starts.
 * @param stack_base Lowest address of a writable stack range.
 * @param stack_size Stack size in bytes (at least the kernel minimum, 8 KiB).
 * @return The thread id, or `NO_PROCESS` when the entry is not a user address, the stack is
 *         smaller than the minimum or not writable user memory, or no thread could be
 *         created (for example the per process thread limit was reached).
 */
inline uint32_t thread_create_raw(uintptr_t entry, void *stack_base,
								  size_t stack_size) {
	uintptr_t result = syscall(SYS_THREAD_CREATE, entry,
							   reinterpret_cast<uintptr_t>(stack_base), stack_size,
							   0, 0, 0);
	return result == FAILURE ? NO_PROCESS : static_cast<uint32_t>(result);
}

/**
 * @brief Blocks while a word has an expected value (`SYS_FUTEX_WAIT`).
 *
 * The kernel compares the word with `expected` with interrupts off: when they differ it
 * returns at once; when equal the thread blocks until woken or the time ends.
 * @param word 4 byte aligned user address; waiters are keyed by process and address.
 * @param expected Value that makes the thread wait.
 * @param milliseconds Longest wait; `FUTEX_NO_TIMEOUT` waits without limit, `0` never
 *        waits (when the word matches it returns the time out result at once). Any other
 *        value above `0x7FFFFFFF` fails.
 * @return `0` (`FUTEX_WOKEN`) when woken, `1` (`FUTEX_VALUE_CHANGED`) when the word did not
 *         match, `2` (`FUTEX_TIMED_OUT`) when the time ran out, `FAILURE` when the address
 *         is null, unaligned or not readable user memory, or the time is above the limit.
 */
inline uintptr_t futex_wait(const uint32_t *word, uint32_t expected,
							uint32_t milliseconds = abi::process::FUTEX_NO_TIMEOUT) {
	return syscall(SYS_FUTEX_WAIT, reinterpret_cast<uintptr_t>(word), expected,
				   milliseconds, 0, 0, 0);
}

/**
 * @brief Wakes threads of this process that wait on a word (`SYS_FUTEX_WAKE`).
 * @param word Same address given to `futex_wait`.
 * @param count Most waiters to wake.
 * @return The number of threads woken (`0` when none waited), or `FAILURE` when the address
 *         is null, unaligned or not readable user memory.
 */
inline uintptr_t futex_wake(const uint32_t *word, uint32_t count) {
	return syscall(SYS_FUTEX_WAKE, reinterpret_cast<uintptr_t>(word), count, 0, 0, 0,
				   0);
}

/**
 * @brief Tells whether an id names a living thread (`SYS_PROC_ALIVE`).
 *
 * A process id is the id of its first thread, so it tells whether that thread is alive;
 * a thread id from `thread_create_raw` works too.
 * @param pid Process or thread id.
 * @return true when the thread exists and is not dead; false when it ended or never existed.
 */
inline bool process_alive(uint32_t pid) {
	return syscall(SYS_PROC_ALIVE, pid, 0, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Writes text to the console (`SYS_WRITE` on `STDOUT_FILE`), looping on partial writes.
 *
 * The kernel takes at most 128 bytes per call and forwards them to the display server
 * (or writes them to the kernel console when there is none), so a long text needs several
 * calls; this function issues them.
 * @param text Bytes to write.
 * @param length Number of bytes.
 * @return The number of bytes written. When a step fails after some bytes were written
 *         the count so far is returned; when the first step fails the raw result (`-1`
 *         for a bad pointer) is returned.
 */
inline long write_text(const char *text, size_t length) {
	size_t written = 0;
	while (written < length) {
		long step = static_cast<long>(
			syscall(SYS_WRITE, STDOUT_FILE,
					reinterpret_cast<uintptr_t>(text + written), length - written,
					0, 0, 0));
		if (step <= 0) return written == 0 ? step : static_cast<long>(written);
		written += static_cast<size_t>(step);
	}
	return static_cast<long>(written);
}

/**
 * @brief Ends the calling process (`SYS_EXIT`). Never returns.
 * @param status Exit status; a spawner that asked for it gets it in `PROCESS_EXITED`.
 */
[[noreturn]] inline void exit_process(uintptr_t status) {
	syscall(SYS_EXIT, status, 0, 0, 0, 0, 0);
	for (;;) {
	}
}

/**
 * @brief Reads one byte from an I/O port (`inb`).
 * @param port Port number; the process must have obtained it with `request_ports`,
 *        otherwise the CPU raises a fault and the process is killed.
 * @return The byte read.
 */
inline uint8_t port_in8(uint16_t port) {
	uint8_t value;
	__asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

/**
 * @brief Writes one byte to an I/O port (`outb`).
 * @param port Port number; needs `request_ports` first, or the process faults.
 * @param value Byte to write.
 */
inline void port_out8(uint16_t port, uint8_t value) {
	__asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * @brief Reads 16 bit words from an I/O port into memory (`rep insw`).
 * @param port Port number; needs `request_ports` first, or the process faults.
 * @param buffer Destination, at least `words * 2` bytes.
 * @param words Number of 16 bit words.
 */
inline void port_read_words(uint16_t port, void *buffer, size_t words) {
	__asm__ volatile("rep insw" : "+D"(buffer), "+c"(words) : "d"(port) : "memory");
}

/**
 * @brief Writes 16 bit words from memory to an I/O port (`rep outsw`).
 * @param port Port number; needs `request_ports` first, or the process faults.
 * @param buffer Source, at least `words * 2` bytes.
 * @param words Number of 16 bit words.
 */
inline void port_write_words(uint16_t port, const void *buffer, size_t words) {
	__asm__ volatile("rep outsw" : "+S"(buffer), "+c"(words) : "d"(port) : "memory");
}

} // namespace cock::user

#endif // !COCK_USER_API_HPP
