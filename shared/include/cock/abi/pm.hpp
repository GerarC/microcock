/**
 * @file pm.hpp
 * @brief Wire protocol of the process manager (`pm`): who creates processes, and the table it keeps of them.
 *
 * `pm` is the kernel parent of every program started after it, so the kernel sends it the exit notice of each
 * one, and it is the one that calls `SYS_PROC_CREATE`. It holds the right to create processes and to label
 * them with a program label, and nothing else: ports, interrupt lines and device memory stay with the root
 * server (`rs`). A caller reaches it through a handle on its endpoint; the badge of the handle is the process id
 * of the caller, which `pm` uses to decide who may ask for what. The calls are synchronous, four words each way.
 * Word 0 of a request is the operation in bits 0 to 7; word 0 of a reply is the `Status` in bits 0 to 7 and,
 * for `LIST`, the state of the row in bits 8 to 15.
 *
 * Who is served: the root server for every operation; the process that runs as `init` (the row with `INIT_PID`)
 * for `ALLOCATE` and `SPAWN`, with a label the root server permitted and with the rights `pm` chooses (no IPC
 * labels, the initial handle on the name server), so `init` can start the programs the manifest gives it and
 * nothing it could use to pass for a server; anyone with a handle for `LIST`. When `init` asks for a process,
 * `pm` tells the root server with a `PM_SPAWNED` message, so it can register the process with the name server
 * and equip it.
 *
 * A program image does not fit in four words, so it travels in a shared memory object that `pm` makes and
 * sizes itself: the caller asks for room with `ALLOCATE`, maps the object, writes a `SpawnRequest` in its first
 * page and the image from `IMAGE_OFFSET` on, and sends `SPAWN`. The size of the object is `pm`'s own, so a
 * caller cannot make `pm` read past it by lying.
 */
#ifndef COCK_ABI_PM_HPP
#define COCK_ABI_PM_HPP

#include <cock/abi/call.hpp>
#include <cock/abi/manifest.hpp>
#include <cock/abi/service.hpp>
#include <cock/abi/shm.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::pm {

/** Operation in the low byte of word 0 of a request. */
enum class Op : uint32_t {
	/** Ask `pm` for a shared object of `bytes` bytes (word 1), shared with the caller for writing; the reply carries its id in word 1. A caller has one at a time. */
	ALLOCATE = 1,
	/** Create a process from the object of word 1, whose first page holds a `SpawnRequest`. The reply carries the kernel process id in word 1 and the process id of the table in word 2. */
	SPAWN = 2,
	/** Root only: a process that already exists is added to the table. Word 1 is its kernel process id, word 2 the kernel process id of its parent or `NO_PARENT`, word 3 the process id of the table it must get (`INIT_PID` for `init`) or `NO_PID` for the next free one. The reply carries the process id of the table in word 1. */
	REGISTER = 3,
	/** Read one row: word 1 is the index. The reply carries the kernel process id in word 1, the process id of the table in word 2 and the parent in word 3. The state is in bits 8 to 15 of word 0: `RUNNING` or `SLEEPING` for a live process, as the kernel reports its threads at that moment, and `EXITED` for one that ended. */
	LIST = 4,
	/** Root only: the process that runs as `init` may create processes with the service label of word 1. A label is given once; the list holds `MAX_PERMITTED_LABELS`. */
	PERMIT = 5,
};

/** Status in the low byte of word 0 of a reply. */
enum class Status : uint32_t {
	/** Done. */
	OK = 0,
	/** The caller may not ask for this. */
	DENIED = 1,
	/** Unknown operation, a size or an index out of range, a name that does not end, an object that was not asked for. */
	BAD_REQUEST = 2,
	/** The kernel refused the process (bad image, rights, no memory or no process id). */
	FAILED = 3,
	/** The table, the list of objects or the memory of `pm` is full. */
	NO_ROOM = 4,
	/** `LIST` past the last row, or `REGISTER` of a parent that is not in the table. */
	NOT_FOUND = 5,
};

/**
 * State of a row of the table, with the letter Linux prints for it in `ps`.
 *
 * The table stores `RUNNING` for a process that lives and `EXITED` for one whose exit notice arrived. `LIST`
 * reports `SLEEPING` instead of `RUNNING` when every live thread waits, which `pm` asks the kernel each time
 * (`SYS_PROC_STATE`) and does not store. `STOPPED`, `ZOMBIE` and `UNINTERRUPTIBLE` are reserved: no value
 * changes and no word of the protocol changes when a stage starts to produce them.
 */
enum class State : uint32_t {
	/** The row is unused. */
	FREE = 0,
	/** `R`: at least one live thread is ready or running; also what `LIST` reports for a live process whose threads the kernel cannot read. */
	RUNNING = 1,
	/** `X`: the exit notice arrived; the row stays until another process needs the room. */
	EXITED = 2,
	/** `S`: every live thread waits for a message, a timer, a futex, a call or a reply, a wait that a signal will be able to end. */
	SLEEPING = 3,
	/** `T`: stopped by a signal. Reserved for `kill` and signals (L3); nothing produces it yet. */
	STOPPED = 4,
	/** `Z`: every thread exited and the parent did not collect the status. Reserved for `wait` (L1); nothing produces it yet. */
	ZOMBIE = 5,
	/** `D`: a thread waits in a call that a signal must not end. Reserved: the first wait of this kind sets it and says so in its header; nothing produces it yet. */
	UNINTERRUPTIBLE = 6,
};

/** Number of words of a request or a reply (the synchronous call width). */
constexpr size_t CALL_WORDS = call::WORD_COUNT;
/** Index of the operation in a request, and of the status in a reply. */
constexpr size_t HEADER_WORD = 0;
/** Index of the first argument of a request, and of the first value of a reply. */
constexpr size_t FIRST_WORD = 1;
/** Index of the second argument or value. */
constexpr size_t SECOND_WORD = 2;
/** Index of the third argument or value. */
constexpr size_t THIRD_WORD = 3;

/** Bits in a byte; shift of the state in the header of a reply. */
constexpr uint32_t BYTE_BITS = 8;
/** Mask of one byte. */
constexpr uint32_t BYTE_MASK = 0xFF;

/** Kernel process id that stands for no parent in `REGISTER`. */
constexpr uint32_t NO_PARENT = 0xFFFFFFFF;
/** Process id of the table that stands for none. */
constexpr uint32_t NO_PID = 0;
/** Process id of the table reserved for `init`, the first process of the system as Linux sees it. */
constexpr uint32_t INIT_PID = 1;
/** Number of labels of programs, from `service::PROGRAM_SERVICE_BASE`: the labels `pm` may give a new process. */
constexpr uint32_t PROGRAM_LABEL_COUNT = manifest::MAX_ENTRIES;
/** First process id of the table given to any other process. */
constexpr uint32_t FIRST_FREE_PID = 2;

/** Longest process name in a `SpawnRequest`, with the terminator. */
constexpr size_t NAME_BYTES = 32;
/** Flag of `SpawnRequest::flags`: `pm` sends the root server a `PM_EXITED` message when the process ends. */
constexpr uint32_t FORWARD_EXIT = 1;
/** Flag of `SpawnRequest::flags`, root only: the new process is `init` and takes `INIT_PID`; refused while another `init` runs. */
constexpr uint32_t AS_INIT = 2;
/** Mask of the flags `pm` knows. */
constexpr uint32_t KNOWN_FLAGS = FORWARD_EXIT | AS_INIT;
/** Most labels the root server may permit to `init`. */
constexpr size_t MAX_PERMITTED_LABELS = manifest::MAX_ENTRIES;
/** Offset of the program image in the object: the request takes the first page. */
constexpr size_t IMAGE_OFFSET = shm::PAGE_BYTES;
/** Largest image `SPAWN` accepts in bytes, the rest of a largest object. */
constexpr size_t MAX_IMAGE_BYTES = shm::MAX_OBJECT_BYTES - IMAGE_OFFSET;

/** First page of the object a caller writes before it sends `SPAWN`. */
struct SpawnRequest {
	/** Name of the process, ending with a zero byte within `NAME_BYTES`. */
	char name[NAME_BYTES];
	/** Rights of the new process, applied before its first thread runs. `pm` accepts a label only from the root server. */
	service::SpawnRights rights;
	/** Bits of `FORWARD_EXIT`; any other bit is refused. */
	uint32_t flags;
	/** Size of the image in bytes, from 1 to `MAX_IMAGE_BYTES` and within the object. */
	uint32_t imageBytes;
};

static_assert(sizeof(SpawnRequest) <= IMAGE_OFFSET, "the request must fit in the first page");

/**
 * @brief Builds the header word of a request.
 * @param op The operation.
 * @return The operation in bits 0 to 7.
 */
inline uint32_t pack(Op op) {
	return static_cast<uint32_t>(op) & BYTE_MASK;
}

/**
 * @brief Extracts the operation of a request header.
 * @param header Word 0 of a request.
 * @return Bits 0 to 7, not validated: compare it with the `Op` values.
 */
inline uint32_t opOf(uint32_t header) {
	return header & BYTE_MASK;
}

/**
 * @brief Builds the header word of a reply.
 * @param status The status.
 * @param state State of the row, for `LIST`; `State::FREE` otherwise.
 * @return The status in bits 0 to 7 and the state in bits 8 to 15.
 */
inline uint32_t packReply(Status status, State state = State::FREE) {
	return (static_cast<uint32_t>(status) & BYTE_MASK) |
		   ((static_cast<uint32_t>(state) & BYTE_MASK) << BYTE_BITS);
}

/**
 * @brief Extracts the status of a reply header.
 * @param header Word 0 of a reply.
 * @return Bits 0 to 7.
 */
inline Status statusOf(uint32_t header) {
	return static_cast<Status>(header & BYTE_MASK);
}

/**
 * @brief Extracts the state of a reply header.
 * @param header Word 0 of a reply.
 * @return Bits 8 to 15.
 */
inline State stateOf(uint32_t header) {
	return static_cast<State>((header >> BYTE_BITS) & BYTE_MASK);
}

} // namespace cock::abi::pm

#endif // !COCK_ABI_PM_HPP
