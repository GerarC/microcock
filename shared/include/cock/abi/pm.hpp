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
 * nothing it could use to pass for a server; anyone with a handle for `LIST` and for `WAIT`, which only
 * reaches the children of the caller. When `init` asks for a process,
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
#include <cock/abi/exec.hpp>
#include <cock/abi/manifest.hpp>
#include <cock/abi/service.hpp>
#include <cock/abi/shm.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::abi::pm {

/** Operation in the low byte of word 0 of a request. */
enum class Op : uint32_t {
	/** Ask `pm` for a shared object of `bytes` bytes (word 1), shared with the caller for writing; the reply carries its id in word 1. A caller has one at a time. Any process with a row in the table may ask, for `EXEC`; only the root server and `init` may then use it for `SPAWN`. */
	ALLOCATE = 1,
	/** Create a process from the object of word 1, whose first page holds a `SpawnRequest`. The reply carries the kernel process id in word 1 and the process id of the table in word 2. */
	SPAWN = 2,
	/** Root only: a process that already exists is added to the table. Word 1 is its kernel process id, word 2 the kernel process id of its parent or `NO_PARENT`, word 3 the process id of the table it must get (`INIT_PID` for `init`) or `NO_PID` for the next free one. The reply carries the process id of the table in word 1. */
	REGISTER = 3,
	/** Read one row: word 1 is the index. The reply carries the kernel process id in word 1, the process id of the table in word 2 and the parent in word 3. The state is in bits 8 to 15 of word 0: `RUNNING` or `SLEEPING` for a live process, as the kernel reports its threads at that moment, and `EXITED` for one that ended. */
	LIST = 4,
	/** Root only: the process that runs as `init` may create processes with the service label of word 1. A label is given once; the list holds `MAX_PERMITTED_LABELS`. */
	PERMIT = 5,
	/** Collect the status of a child: word 1 is its process id of the table or `ANY_CHILD`. A child that exited and was not collected answers at once; otherwise the call waits, with the caller blocked, until a matching child exits. The reply carries the process id of the child in word 1 and its exit status in word 2. `NOT_FOUND` means the caller has no such child, collected or not. The root server has none to collect: `pm` collects its children itself. */
	WAIT = 6,
	/** Replace the program image of the caller with the one in the object of word 1, whose first page holds an `ExecRequest`; the caller got the object with `ALLOCATE`. On success the call never returns: the caller starts again in the new image, with the same process id, parent, capabilities and endpoints, and its other threads end. On failure the reply carries the status and the old image runs on. */
	EXEC = 7,
	/** Make a copy of the caller: a new process with the address space of the caller shared copy on write, the `IPC` and endpoint capabilities of the caller (with the handle numbers they had, so the numbers the program stored still work; a handle whose badge is the id of the caller gets the id of the child, so a server tells the two apart and keeps its own session for each), no service label, and the caller as its parent in the table. Both continue inside this call. The reply to the caller carries the process id of the table of the child in word 1; the child gets the same reply with word 1 equal to `NO_PID`. `pm` tells the root server with a `PM_FORKED` message, which sends it to the programs that keep state for each process (`notify=fork`), so the child has the files and pipe ends of its parent, and tells the name server, so the child may connect by name to what its parent may and to nothing else; `pm` answers the caller only when the root server says it did (`PM_FORK_SETTLED`), or after `MAX_SETTLE_TIMEOUTS` waits of `NOTICE_RETRY_MILLISECONDS`, so the caller cannot change what the child inherits. The caller needs a row in the table. `FAILED` when the kernel refused (no process id, no memory for the page tables, too many children with an exit notice, a shared object with no holder slot, a derivation tree with no room). */
	FORK = 8,
	/** Send a signal to a process: word 1 is its process id of the table, word 2 the signal (`abi::process::SIGNAL_INTERRUPT`, `SIGNAL_TERMINATE` or `SIGNAL_KILL`). Interrupt and terminate reach the main thread of the process as a `SIGNAL` message, which the program may ignore or answer by ending; kill ends the process at once, whatever it does, and its parent collects the status `abi::process::KILLED_STATUS`. The root server may signal any process; any other caller only its own descendants (its children, theirs, and so on), never itself. `DENIED` for any other target, `NOT_FOUND` for a process that is not in the table or already ended, `BAD_REQUEST` for an unknown signal, `FAILED` when the kernel refused (the inbox of the target is full, or the target is the caller). */
	KILL = 9,
	/** Write the whole table in the object of word 1, which the caller got with `ALLOCATE` for at least `sizeof(Snapshot)` bytes: the object starts with a `Snapshot`. The reply carries the number of rows in word 1. `pm` gives its own share of the object back after writing, so the caller maps it, reads it and releases it; a new `ALLOCATE` is needed for the next one. The state of each row is the one `LIST` reports at that moment. `BAD_REQUEST` for an object the caller does not hold or one too small. */
	SNAPSHOT = 10,
	/** Root only: give a process that is in the table a name. Bits 8 to 15 of word 0 are the chunk number, word 1 is the kernel process id, words 2 and 3 are the `NAME_CHUNK_BYTES` bytes of that chunk of the name, which ends with a zero byte within `NAME_BYTES`; the chunks of a name are sent in order from 0, and the last one the name needs is the one that holds the zero byte. `NOT_FOUND` for a process that is not in the table, `BAD_REQUEST` for a chunk past `NAME_CHUNKS`. */
	SET_NAME = 11,
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
	/** `LIST` past the last row, `REGISTER` of a parent that is not in the table, or `WAIT` with no child to wait for. */
	NOT_FOUND = 5,
};

/**
 * State of a row of the table, with the letter Linux prints for it in `ps`.
 *
 * The table stores `RUNNING` for a process that lives and `EXITED` for one whose exit notice arrived. `LIST`
 * reports `SLEEPING` instead of `RUNNING` when every live thread waits, which `pm` asks the kernel each time
 * (`SYS_PROC_STATE`) and does not store; it does the same for `STOPPED`. `UNINTERRUPTIBLE` is reserved: no value
 * changes and no word of the protocol changes when a stage starts to produce it.
 */
enum class State : uint32_t {
	/** The row is unused. */
	FREE = 0,
	/** `R`: at least one live thread is ready or running; also what `LIST` reports for a live process whose threads the kernel cannot read. */
	RUNNING = 1,
	/** `X`: gone. The exit notice arrived and nobody has to collect the status (the parent collected it, the parent is the root server or the parent exited first); the row stays until another process needs the room. */
	EXITED = 2,
	/** `S`: every live thread waits for a message, a timer, a futex, a call or a reply, a wait that a signal will be able to end. */
	SLEEPING = 3,
	/** `T`: stopped by `SIGNAL_STOP`: no thread of the process runs until `SIGNAL_CONTINUE`. `LIST` reports it from the kernel each time, like `SLEEPING`, and the table does not store it. */
	STOPPED = 4,
	/** `Z`: every thread exited and the parent, which lives, did not collect the status with `WAIT` yet. The row keeps the process id and the status and is not reused; collecting it, or the exit of the parent, turns it into `EXITED`. */
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
/** Process id in a `WAIT` request that stands for any child. */
constexpr uint32_t ANY_CHILD = 0xFFFFFFFF;
/** First process id of the table given to any other process. */
constexpr uint32_t FIRST_FREE_PID = 2;
/** Name a copy made by `fork` connects to when it starts, only to wait until the name server knows it: no program has it, so the answer is `NOT_FOUND` once the copy is known. */
constexpr char FORK_SETTLE_NAME[] = "/run/forked";

/** Longest process name in a `SpawnRequest`, with the terminator. */
constexpr size_t NAME_BYTES = 32;
/** Words of a `SET_NAME` request that carry the name, the last two. */
constexpr size_t NAME_CHUNK_WORDS = 2;
/** Bytes of a name that one `SET_NAME` request carries. */
constexpr size_t NAME_CHUNK_BYTES = NAME_CHUNK_WORDS * sizeof(uint32_t);
/** Number of chunks of a name of `NAME_BYTES` bytes. */
constexpr size_t NAME_CHUNKS = NAME_BYTES / NAME_CHUNK_BYTES;
static_assert(NAME_CHUNKS * NAME_CHUNK_BYTES == NAME_BYTES, "a name must be a whole number of chunks");
/** Most rows the table of `pm` holds, live and exited; also the most rows of a `Snapshot`. */
constexpr size_t MAX_PROCESSES = 128;
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
/** Largest image `EXEC` accepts: the same, as long as it ends below the argument page. */
constexpr size_t MAX_EXEC_IMAGE_BYTES = MAX_IMAGE_BYTES;

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

/** First page of the object a caller writes before it sends `EXEC`. */
struct ExecRequest {
	/** Name of the new image, ending with a zero byte within `NAME_BYTES`. */
	char name[NAME_BYTES];
	/** Size of the image in bytes, from 1 to `MAX_EXEC_IMAGE_BYTES` and within the object. */
	uint32_t imageBytes;
	/** The arguments and the initial handle the new image receives; `pm` checks the whole block. */
	exec::ArgsBlock args;
};

static_assert(sizeof(ExecRequest) <= IMAGE_OFFSET, "the request must fit in the first page");

/** One row of a `Snapshot`: what `LIST` gives for a process, with its name and exit status. */
struct ProcessRecord {
	/** Id of the process in the kernel. */
	uint32_t kernelId;
	/** Id of the process in the table of `pm`. */
	uint32_t pid;
	/** Id in the table of the parent, or `NO_PID`. */
	uint32_t parent;
	/** State as `LIST` reports it. */
	State state;
	/** Status the process exited with; valid when `state` is `EXITED` or `ZOMBIE`. */
	uint32_t exitStatus;
	/** Name the process was created with, or the one `SET_NAME` gave; ends with a zero byte, empty when it has none. */
	char name[NAME_BYTES];
};

/** What `SNAPSHOT` writes at the start of the object: the number of rows and the rows, in the order of `LIST`. */
struct Snapshot {
	/** Rows in use, from 0 to `MAX_PROCESSES`. */
	uint32_t count;
	/** The first `count` entries are valid. */
	ProcessRecord records[MAX_PROCESSES];
};

static_assert(sizeof(Snapshot) <= shm::MAX_OBJECT_BYTES, "a snapshot must fit in one shared object");

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
 * @brief Builds the header word of a `SET_NAME` request.
 * @param chunk Number of the chunk of the name, from 0 to `NAME_CHUNKS` - 1.
 * @return The operation in bits 0 to 7 and the chunk in bits 8 to 15.
 */
inline uint32_t packNameChunk(uint32_t chunk) {
	return pack(Op::SET_NAME) | ((chunk & BYTE_MASK) << BYTE_BITS);
}

/**
 * @brief Extracts the chunk number of a request header.
 * @param header Word 0 of a request.
 * @return Bits 8 to 15, not validated: compare it with `NAME_CHUNKS`.
 */
inline uint32_t chunkOf(uint32_t header) {
	return (header >> BYTE_BITS) & BYTE_MASK;
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
