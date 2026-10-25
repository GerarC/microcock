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
#include <cock/abi/endpoint.hpp>
#include <cock/abi/process.hpp>
#include <cock/abi/sched_metrics.hpp>
#include <cock/abi/service.hpp>
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
/** Value of the `flags` argument of `SYS_CAP_GRANT` for a capability the target cannot pass on. */
constexpr uintptr_t NO_GRANT_FLAGS = 0;
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
 * @brief Sends an asynchronous message to a thread of the caller's own process (`SYS_IPC_SEND`).
 * Never blocks.
 *
 * The inbox belongs to a thread, not to the process: a process id is the id of its first
 * thread, so the message reaches that thread, and only that thread receives it. A thread of
 * another process is not reachable this way: use a handle (`ep_send`).
 * @param pid Id of the target thread, which must belong to the calling process.
 * @param message Message to copy; the kernel stamps `target_pid` with the id of the receiving
 *        thread and clears `badge` and `reply`.
 * @return true when the message was queued; false when the target does not exist, is dead or
 *         belongs to another process, its inbox (16 messages) is full, the pointer is bad or the
 *         message type is not allowed for this sender (`IRQ_NOTIFICATION` and `PROCESS_EXITED`
 *         never; `CAPABILITY_GRANTED` only with grant authority). Nothing is retried. A
 *         `CAPABILITY_TRANSFER` message is sent with `cap_send`, because a successful send of
 *         one returns the node id and this wrapper reads only zero as success.
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
 * @param delegate true to let the target pass the capability on (`abi::GRANT_DELEGATE`): it holds
 *        the grant flag for the range, as the name server does for `IPC` capabilities. false
 *        gives a capability the target cannot pass on.
 * @param mint `ENDPOINT` only: true to let the target choose the badge of the handles it grants
 *        (`abi::GRANT_MINT`). The caller must hold the right itself.
 * @param badge `ENDPOINT` only: the badge of the handle that the target gets. Only a caller that
 *        holds the mint right may pass a value other than `abi::endpoint::NO_BADGE`.
 * @return The id of the node of the derivation tree that records the grant (greater than 0),
 *         or `abi::NO_CAPABILITY_NODE` when the type is unknown, the caller holds no grantable
 *         capability covering the range, the target does not exist or is a kernel process,
 *         the capability table of the target is full, or the derivation tree is full. For an
 *         `ENDPOINT` capability the node id is the handle in the table of the target.
 */
inline uint32_t cap_grant_node(uint32_t pid, abi::CapabilityType type, uintptr_t first,
							   uintptr_t count, bool delegate = false, bool mint = false,
							   uint32_t badge = abi::endpoint::NO_BADGE) {
	uintptr_t flags = (delegate ? abi::GRANT_DELEGATE : NO_GRANT_FLAGS) |
					  (mint ? abi::GRANT_MINT : NO_GRANT_FLAGS);
	uintptr_t result = syscall(SYS_CAP_GRANT, pid, static_cast<uintptr_t>(type), first,
							   count, flags, badge);
	return result == FAILURE ? abi::NO_CAPABILITY_NODE : static_cast<uint32_t>(result);
}

/**
 * @brief Same as `cap_grant_node`, for callers that do not need the node id.
 * @return true when granted; false in every case where `cap_grant_node` fails.
 */
inline bool cap_grant(uint32_t pid, abi::CapabilityType type, uintptr_t first,
					  uintptr_t count, bool delegate = false, bool mint = false) {
	return cap_grant_node(pid, type, first, count, delegate, mint) != abi::NO_CAPABILITY_NODE;
}

/**
 * @brief Chooses which kinds of capability a message may give the caller (`SYS_CAP_ACCEPT`).
 *
 * A process accepts nothing until it calls this, so a `CAPABILITY_TRANSFER` message sent to it is
 * refused. The mask replaces the previous one; call it again with `abi::NO_CAPABILITY_TYPES` to stop
 * accepting.
 * @param mask Bits of `abi::capabilityBit(type)`, with no bit outside `abi::ALL_CAPABILITY_TYPES`.
 * @return true when set; false when the mask has a bit that is not a capability type.
 */
inline bool cap_accept(uint32_t mask) {
	return syscall(SYS_CAP_ACCEPT, mask, 0, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Builds the message that gives a capability (`CAPABILITY_TRANSFER`), for `cap_send` and `cap_send_local`.
 * @param type Kind of capability.
 * @param first First resource of the range.
 * @param count Number of resources.
 * @param delegate true to let the receiver pass the copy on.
 * @param tag Value that arrives in `arg5`.
 * @param mint `ENDPOINT` only: true to let the receiver choose badges.
 * @return The message.
 */
inline ipc::Message capability_message(abi::CapabilityType type, uint32_t first, uint32_t count,
									   bool delegate, uint32_t tag, bool mint) {
	ipc::Message message = {};
	message.type = ipc::MessageType::CAPABILITY_TRANSFER;
	message.payload.args.arg0 = static_cast<uint32_t>(type);
	message.payload.args.arg1 = first;
	message.payload.args.arg2 = count;
	message.payload.args.arg3 = (delegate ? abi::GRANT_DELEGATE : NO_GRANT_FLAGS) |
								(mint ? abi::GRANT_MINT : NO_GRANT_FLAGS);
	message.payload.args.arg5 = tag;
	return message;
}

/**
 * @brief Sends a capability the caller holds to another process in a message, through a handle
 * (`SYS_EP_SEND`).
 *
 * The kernel derives a copy of part of a grantable capability of the caller for the process bound
 * to the endpoint of the handle and records it in the derivation tree, so a holder of an ancestor
 * can revoke it. The send fails, and nothing is given, when the caller holds no such handle, holds
 * no grantable capability that covers the range, the receiver did not accept the type with
 * `cap_accept`, the receiver is the caller, or a table is full.
 * @param handle A handle on the endpoint of the receiver.
 * @param type Kind of capability.
 * @param first First resource of the range.
 * @param count Number of resources.
 * @param delegate true to let the receiver pass the copy on.
 * @param tag Free value that arrives in the message (`arg5`); for an `ENDPOINT` capability it is
 *        the badge of the handle that the receiver gets, and only a sender with the mint right may
 *        pass a value other than zero.
 * @param mint `ENDPOINT` only: true to let the receiver choose badges (`abi::GRANT_MINT`).
 * @return The id of the node of the derivation tree that records the copy (greater than 0), which
 *         the caller can pass to `cap_revoke`; `abi::NO_CAPABILITY_NODE` when the send failed. The
 *         receiver finds the same id in `arg4` of the message and the badge of the handle in `badge`.
 */
inline uint32_t cap_send(uint32_t handle, abi::CapabilityType type, uint32_t first, uint32_t count,
						 bool delegate = false, uint32_t tag = 0, bool mint = false) {
	ipc::Message message = capability_message(type, first, count, delegate, tag, mint);
	uintptr_t result = syscall(SYS_EP_SEND, handle, reinterpret_cast<uintptr_t>(&message),
							   abi::endpoint::NO_SEND_FLAGS, 0, 0, 0);
	return result == FAILURE ? abi::NO_CAPABILITY_NODE : static_cast<uint32_t>(result);
}

/**
 * @brief Sends a capability message by thread id (`SYS_IPC_SEND`). The kernel refuses it unless the
 * thread belongs to the caller's own process, and a capability is never given to oneself, so it
 * always returns `abi::NO_CAPABILITY_NODE`; the test programs use it to check that.
 * @param pid Id of a thread.
 * @param type Kind of capability.
 * @param first First resource of the range.
 * @param count Number of resources.
 * @return `abi::NO_CAPABILITY_NODE` in every case a cross process send would be refused.
 */
inline uint32_t cap_send_local(uint32_t pid, abi::CapabilityType type, uint32_t first, uint32_t count) {
	ipc::Message message = capability_message(type, first, count, false, 0, false);
	uintptr_t result = syscall(SYS_IPC_SEND, pid, reinterpret_cast<uintptr_t>(&message), 0, 0, 0, 0);
	return result == FAILURE ? abi::NO_CAPABILITY_NODE : static_cast<uint32_t>(result);
}

/**
 * @brief Gives a process its service label (`SYS_SERVICE_SET`), the stable name under which IPC
 * capabilities authorize communication with it.
 *
 * The label can be set once per process. The caller needs grant authority.
 * @param pid Process id of a live user process.
 * @param label Label, not `abi::service::NO_SERVICE`.
 * @return true when set; false when the caller has no grant authority, the process is unknown or
 *         is a kernel process, the label is `NO_SERVICE`, or the process already has a label.
 */
inline bool service_set(uint32_t pid, uint32_t label) {
	return syscall(SYS_SERVICE_SET, pid, label, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Creates an endpoint (`SYS_EP_CREATE`) and takes the capability that owns it.
 *
 * The owner capability has the grant, mint and bind rights. It lets the caller bind the endpoint
 * to a process with `ep_bind` and hand out handles with `cap_grant_node` or `cap_send`, choosing the
 * badge of each. The caller needs grant authority.
 * @param handle Receives the handle of the owner capability, which `ep_send` and `ep_call` accept;
 *        `abi::NO_CAPABILITY_NODE` when the caller already held a capability that covers the new
 *        endpoint with the bind right (the first program holds one over all of them).
 * @return The endpoint id (greater than 0), or `abi::endpoint::NO_ENDPOINT` when the caller has no
 *         grant authority or a table is full.
 */
inline uint32_t ep_create(uint32_t &handle) {
	uintptr_t result = syscall(SYS_EP_CREATE, reinterpret_cast<uintptr_t>(&handle), 0, 0, 0, 0, 0);
	return result == FAILURE ? abi::endpoint::NO_ENDPOINT : static_cast<uint32_t>(result);
}

/**
 * @brief Binds an endpoint to the process that receives what is sent to it (`SYS_EP_BIND`).
 *
 * A later call replaces the owner, which is how a restarted server takes over the endpoint that its
 * clients hold handles on.
 * @param endpoint Endpoint id. The caller needs the bind right of its capability.
 * @param pid Process id of a live user process.
 * @return true when bound; false when the right is missing, the endpoint does not exist or the
 *         process is unknown, not a user process or not alive.
 */
inline bool ep_bind(uint32_t endpoint, uint32_t pid) {
	return syscall(SYS_EP_BIND, endpoint, pid, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Sends an asynchronous message through a handle (`SYS_EP_SEND`).
 *
 * The kernel writes the badge of the handle in `Message::badge`, whatever the message holds.
 * @param handle A handle the caller holds.
 * @param message The message; the sender, target, badge and reply fields are overwritten.
 * @param expectReply true to let the receiver answer: the kernel opens a reply token and puts it in
 *        `Message::reply` of the message the receiver gets. The answer comes back as a message in the
 *        inbox of the calling thread (`ep_reply`), whenever the receiver makes it.
 * @return true when queued; false when the handle is not held, the endpoint is unbound or its process
 *         is gone, the inbox is full, the type is reserved, or a token was asked for and the table is
 *         full or the caller already has `abi::endpoint::MAX_REPLIES_PER_REQUESTER` open.
 */
inline bool ep_send(uint32_t handle, const ipc::Message &message, bool expectReply = false) {
	uintptr_t flags = expectReply ? abi::endpoint::SEND_EXPECT_REPLY : abi::endpoint::NO_SEND_FLAGS;
	return syscall(SYS_EP_SEND, handle, reinterpret_cast<uintptr_t>(&message), flags, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Answers a message that carried a reply token (`SYS_EP_REPLY`).
 *
 * The answer arrives in the inbox of the thread that asked, with the id of the calling thread as
 * sender and `abi::endpoint::NO_REPLY` and `NO_BADGE` in the token and badge fields. The token is
 * spent: a second answer fails.
 * @param token `Message::reply` of the request. The calling process must hold it.
 * @param message The answer; a reserved type or a capability message is refused.
 * @return true when queued; false when the process does not hold the token (it was used, forwarded
 *         away, never given or dropped), the thread that asked is gone, or its inbox is full (the
 *         token stays open in that case, so the call can be repeated).
 */
inline bool ep_reply(uint32_t token, const ipc::Message &message) {
	return syscall(SYS_EP_REPLY, token, reinterpret_cast<uintptr_t>(&message), 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Passes a request and its reply token to another endpoint (`SYS_EP_FORWARD`).
 *
 * The process bound to the endpoint receives `message` with the badge of the handle and the same
 * token, and answers the thread that asked directly. The forwarder no longer holds the token.
 * @param token `Message::reply` of the request. The calling process must hold it.
 * @param handle A handle the caller holds on the endpoint that should answer.
 * @param message The request to pass on; reserved types and capability messages are refused.
 * @return true when queued; false when the process does not hold the token, the handle leads nowhere
 *         or the inbox of the receiver is full; the caller keeps the token then.
 */
inline bool ep_forward(uint32_t token, uint32_t handle, const ipc::Message &message) {
	return syscall(SYS_EP_FORWARD, token, handle, reinterpret_cast<uintptr_t>(&message), 0, 0, 0) == SUCCESS;
}

/**
 * @brief Revokes a capability that was granted, and everything derived from it (`SYS_CAP_REVOKE`).
 *
 * The holder loses the capability, and the ports, interrupt lines and device mappings it opened
 * with it are closed unless another capability still covers them.
 * @param node Node id that `cap_grant_node` returned.
 * @return true when revoked; false when the id is unknown, or the caller holds no ancestor of the
 *         node (a node the caller holds itself and the capabilities of the first program cannot
 *         be revoked by their holder).
 */
inline bool cap_revoke(uint32_t node) {
	return syscall(SYS_CAP_REVOKE, node, 0, 0, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Finds the process the kernel started from a boot module (`SYS_MODULE_PROCESS`).
 *
 * Only a process with grant authority may call it; `rs` uses it to find the programs the kernel
 * starts before it runs, such as the name server. Every other program finds a server through the
 * name server (`names.hpp`).
 * @param name NUL terminated boot module name; its length is measured here.
 * @return The process id, or `NO_PROCESS` when no live process was started from a boot module
 *         with that name, the name is empty or too long for the kernel, or the caller is not allowed.
 */
inline uint32_t module_process(const char *name) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result = syscall(SYS_MODULE_PROCESS, reinterpret_cast<uintptr_t>(name),
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
 * Only a process with grant authority may call it; `rs` uses it to grant the
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
 * @brief Creates a process with an empty address space and no thread (`SYS_PROC_CREATE`).
 *
 * Only a process that holds a `SPAWN` capability may call it. The process starts with an empty
 * capability set, except for the rights in `rights`, applied by `proc_start`. `spawn` in
 * `spawn.hpp` uses this call and the next ones to start a program from an ELF image.
 * @param name NUL terminated process name; its length is measured here.
 * @param notify_exit true to receive a `PROCESS_EXITED` message when the process ends once it started; `proc_start` fails when the caller already has `MAX_NOTIFIED_CHILDREN` such children running or unread.
 * @param rights The service label of the new process and the labels it may talk to; null leaves the process with no label and no IPC capability. The caller must hold a grantable `IPC` capability for the label and for each label of the list.
 * @param user_limit First address the process may not use; page aligned and above the user stack.
 * @return The handle of the pending process, or `abi::process::NO_PENDING_PROCESS` when the caller may not spawn, holds `MAX_PENDING_PER_PROCESS` pending processes already, the system holds `MAX_PENDING_PROCESSES`, a pointer or the limit is bad, ids or memory ran out, or the rights are invalid (no label, too many labels, a label the caller cannot give, or the derivation tree is full).
 */
inline uint32_t proc_create(const char *name, bool notify_exit,
							const abi::service::SpawnRights *rights, uintptr_t user_limit) {
	size_t length = 0;
	while (name[length] != '\0')
		length++;

	uintptr_t result =
		syscall(SYS_PROC_CREATE, reinterpret_cast<uintptr_t>(name), length,
				notify_exit ? abi::process::SPAWN_NOTIFY_EXIT : 0,
				reinterpret_cast<uintptr_t>(rights), user_limit, 0);
	return result == FAILURE ? abi::process::NO_PENDING_PROCESS : static_cast<uint32_t>(result);
}

/**
 * @brief Maps fresh zeroed writable pages into a pending process (`SYS_PROC_MAP`).
 * @param handle Handle from `proc_create`.
 * @param address Page aligned start, not in the first page, below the user stack of the process.
 * @param pages Page count; at most `MAX_PENDING_IMAGE_BYTES` in all for the process.
 * @return true when mapped; false for a handle that is not the caller's, a bad or overlapping range, or no memory. A failure can leave pages mapped: call `proc_discard`.
 */
inline bool proc_map(uint32_t handle, uintptr_t address, size_t pages) {
	return syscall(SYS_PROC_MAP, handle, address, pages, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Copies bytes into mapped, still writable pages of a pending process (`SYS_PROC_WRITE`).
 * @param handle Handle from `proc_create`.
 * @param address Destination in the pending process.
 * @param source Source in the caller.
 * @param length Byte count, non zero.
 * @return true when copied; false for a bad handle, an unreadable source or a destination that is not mapped and writable.
 */
inline bool proc_write(uint32_t handle, uintptr_t address, const void *source, size_t length) {
	return syscall(SYS_PROC_WRITE, handle, address, reinterpret_cast<uintptr_t>(source), length,
				   0, 0) == SUCCESS;
}

/**
 * @brief Makes mapped pages of a pending process read only (`SYS_PROC_PROTECT`).
 * @param handle Handle from `proc_create`.
 * @param address Page aligned start.
 * @param pages Page count; every page must be mapped.
 * @return true when protected; false for a bad handle or range.
 */
inline bool proc_protect(uint32_t handle, uintptr_t address, size_t pages) {
	return syscall(SYS_PROC_PROTECT, handle, address, pages, 0, 0, 0) == SUCCESS;
}

/**
 * @brief Starts a pending process at an entry point (`SYS_PROC_START`).
 * @param handle Handle from `proc_create`; it stops being valid on success.
 * @param entry Entry point, inside a mapped page.
 * @return The process id (the id of its first thread), or `NO_PROCESS` when the handle is not the caller's, the entry is not mapped, no id is free, the rights are no longer valid, or the caller has too many children with an exit notice. The process stays pending after a failure.
 */
inline uint32_t proc_start(uint32_t handle, uintptr_t entry) {
	uintptr_t result = syscall(SYS_PROC_START, handle, entry, 0, 0, 0, 0);
	return result == FAILURE ? NO_PROCESS : static_cast<uint32_t>(result);
}

/**
 * @brief Replaces the program image of a process with a pending one (`SYS_PROC_EXEC`).
 *
 * Only a process that holds a `SPAWN` capability may call it; the process manager is the one that does. The
 * target must be waiting for the reply of a call the caller is serving, and its other threads end. The new
 * image keeps the id, the capabilities and the endpoints of the old one and receives `initial` at its entry.
 * @param handle Handle from `proc_create`, made with no rights and no exit notice; it stops being valid on success.
 * @param entry Entry point, inside a mapped page.
 * @param pid Process id of the target.
 * @param initial The one number the new image receives in `EAX`.
 * @return true when the image was replaced; false when the caller may not, the handle or the entry is bad, or the target is not waiting for a reply the caller serves. The old image is untouched after a failure.
 */
inline bool proc_exec(uint32_t handle, uintptr_t entry, uint32_t pid, uintptr_t initial) {
	return syscall(SYS_PROC_EXEC, handle, entry, pid, initial, 0, 0) == SUCCESS;
}

/**
 * @brief Destroys a pending process and frees its memory (`SYS_PROC_DISCARD`).
 * @param handle Handle from `proc_create`.
 * @return true when destroyed; false when the handle is not the caller's.
 */
inline bool proc_discard(uint32_t handle) {
	return syscall(SYS_PROC_DISCARD, handle, 0, 0, 0, 0, 0) == SUCCESS;
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

/** Result of `process_state_mask` for a process that is unknown, belongs to the kernel or has no live thread. */
constexpr uint32_t NO_STATE_MASK = 0;

/**
 * @brief Tells in which states the live threads of a process are (`SYS_PROC_STATE`).
 *
 * @param pid Process id: the id of its first thread, which must not have been reaped yet.
 * @return A mask of the `abi::process::THREAD_*` bits, one for each state a live thread is in, or
 *         `NO_STATE_MASK` when the id is not a process id, names the kernel or has no live thread.
 */
inline uint32_t process_state_mask(uint32_t pid) {
	uintptr_t result = syscall(SYS_PROC_STATE, pid, 0, 0, 0, 0, 0);
	return result == FAILURE ? NO_STATE_MASK : static_cast<uint32_t>(result);
}

/**
 * @brief Gives the process a thread belongs to (`SYS_THREAD_PROCESS`).
 *
 * A server that serves a synchronous call sees the id of the calling thread; this tells it whose
 * thread that is.
 * @param tid Id of a thread; a process id names its first thread.
 * @return The process id, or `NO_PROCESS` when the thread is unknown, ended or belongs to the kernel.
 */
inline uint32_t thread_process(uint32_t tid) {
	uintptr_t result = syscall(SYS_THREAD_PROCESS, tid, 0, 0, 0, 0, 0);
	return result == FAILURE ? NO_PROCESS : static_cast<uint32_t>(result);
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
