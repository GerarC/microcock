/**
 * @file specs.hpp
 * @brief Table that describes the arguments of every system call for the fuzzer.
 *
 * The generator reads it to build plausible arguments (about 70 percent) and hostile ones (about 30 percent) for each call.
 */
#ifndef FUZZ_SPECS_HPP
#define FUZZ_SPECS_HPP

#include <stddef.h>
#include <stdint.h>
#include <sys/syscalls.h>

namespace fuzz {

/** Arguments of a system call (EBX, ECX, EDX, ESI, EDI, EBP). */
constexpr size_t ARGUMENT_COUNT = 6;

/** Kind of an argument; tells the generator how to build a value. */
enum class Arg : uint8_t {
	/** Unused; the value is 0. */
	NONE,
	/** A process or thread id (live, dead or stale). */
	ID,
	/** Pointer to memory the kernel reads (a valid buffer or a bad address). */
	ADDRESS_IN,
	/** Pointer to memory the kernel writes. */
	ADDRESS_OUT,
	/** Any address, valid or not. */
	ADDRESS_ANY,
	/** A 4 byte aligned address (futex words). */
	ADDRESS_ALIGNED,
	/** Start of a region the fuzzer mapped. */
	REGION,
	/** A byte length, small or wrapping. */
	LENGTH,
	/** A length for mmap or munmap. */
	MAP_LENGTH,
	/** A page count. */
	PAGES,
	/** Flag bits or protection bits. */
	FLAGS,
	/** An arbitrary 32 bit value. */
	VALUE,
	/** A time in milliseconds; always finite so a call cannot block the run. */
	MILLISECONDS,
	/** A shared memory object id. */
	SHM,
	/** Pointer to a name. */
	NAME_POINTER,
	/** Length of a name. */
	NAME_LENGTH,
	/** A file descriptor number. */
	FD,
	/** An I/O port number. */
	PORT,
	/** Entry point of a new thread. */
	ENTRY,
	/** Stack base of a new thread. */
	STACK_BASE,
	/** Stack size of a new thread. */
	STACK_SIZE,
	/** Pointer to text for `write`. */
	TEXT_POINTER,
	/** Length of the text for `write`. */
	TEXT_LENGTH,
};

/** Description of one system call. */
struct Spec {
	/** System call number (`SYS_*`). */
	uint32_t number;
	/** How often the call is picked, relative to the others (`RARE`, `COMMON`, `FREQUENT`). */
	uint8_t weight;
	/** Kind of each argument; unused trailing entries are `Arg::NONE`. */
	Arg args[ARGUMENT_COUNT];
};

/** Weight of calls that need little exercise. */
constexpr uint8_t RARE = 1;
/** Weight of ordinary calls. */
constexpr uint8_t COMMON = 4;
/** Weight of calls with the most state to break (memory and IPC). */
constexpr uint8_t FREQUENT = 8;

/** One entry per system call that the fuzzer may use; `exit` is left out. */
constexpr Spec SPECS[] = {
	{SYS_READ, COMMON, {Arg::FD, Arg::ADDRESS_OUT, Arg::LENGTH}},
	{SYS_WRITE, RARE, {Arg::FD, Arg::TEXT_POINTER, Arg::TEXT_LENGTH}},
	{SYS_REQUEST_RESOURCE, COMMON, {Arg::PORT, Arg::VALUE}},
	{SYS_GETPID, RARE, {}},
	{SYS_MMAP, FREQUENT,
	 {Arg::ADDRESS_ANY, Arg::MAP_LENGTH, Arg::FLAGS, Arg::FLAGS, Arg::VALUE, Arg::VALUE}},
	{SYS_MUNMAP, FREQUENT, {Arg::REGION, Arg::MAP_LENGTH}},
	{SYS_SCHED_YIELD, RARE, {}},
	{SYS_IPC_SEND, FREQUENT, {Arg::ID, Arg::ADDRESS_IN}},
	{SYS_IPC_RECV, COMMON, {Arg::ADDRESS_OUT}},
	{SYS_CAP_GRANT, COMMON, {Arg::ID, Arg::VALUE, Arg::VALUE, Arg::VALUE}},
	{SYS_IRQ_BIND, COMMON, {Arg::VALUE}},
	{SYS_IRQ_ACK, COMMON, {Arg::VALUE}},
	{SYS_PROC_LOOKUP, COMMON, {Arg::NAME_POINTER, Arg::NAME_LENGTH}},
	{SYS_SHM_CREATE, FREQUENT, {Arg::PAGES}},
	{SYS_SHM_SHARE, COMMON, {Arg::SHM, Arg::ID, Arg::FLAGS}},
	{SYS_SHM_MAP, FREQUENT, {Arg::SHM, Arg::FLAGS}},
	{SYS_SHM_RELEASE, COMMON, {Arg::SHM}},
	{SYS_MODULE_OPEN, COMMON, {Arg::NAME_POINTER, Arg::NAME_LENGTH}},
	{SYS_MODULE_INDEX, COMMON, {Arg::NAME_POINTER, Arg::NAME_LENGTH}},
	{SYS_SPAWN, COMMON,
	 {Arg::ADDRESS_IN, Arg::LENGTH, Arg::NAME_POINTER, Arg::NAME_LENGTH, Arg::FLAGS}},
	{SYS_PROC_ALIVE, COMMON, {Arg::ID}},
	{SYS_IPC_TRY_RECV, COMMON, {Arg::ADDRESS_OUT}},
	{SYS_IPC_RECV_TIMEOUT, COMMON, {Arg::ADDRESS_OUT, Arg::MILLISECONDS}},
	{SYS_SLEEP, COMMON, {Arg::MILLISECONDS}},
	{SYS_THREAD_CREATE, COMMON, {Arg::ENTRY, Arg::STACK_BASE, Arg::STACK_SIZE}},
	{SYS_FUTEX_WAIT, COMMON, {Arg::ADDRESS_ALIGNED, Arg::VALUE, Arg::MILLISECONDS}},
	{SYS_FUTEX_WAKE, COMMON, {Arg::ADDRESS_ALIGNED, Arg::VALUE}},
	{SYS_GETTID, RARE, {}},
	{SYS_IPC_CALL, FREQUENT,
	 {Arg::ID, Arg::VALUE, Arg::VALUE, Arg::VALUE, Arg::VALUE, Arg::MILLISECONDS}},
	{SYS_IPC_ACCEPT, COMMON,
	 {Arg::NONE, Arg::NONE, Arg::NONE, Arg::NONE, Arg::NONE, Arg::MILLISECONDS}},
	{SYS_IPC_REPLY, COMMON, {Arg::NONE, Arg::VALUE, Arg::VALUE, Arg::VALUE, Arg::VALUE}},
	{SYS_IPC_REPLY_WAIT, COMMON,
	 {Arg::NONE, Arg::VALUE, Arg::VALUE, Arg::VALUE, Arg::VALUE, Arg::MILLISECONDS}},
	{SYS_IPC_DEFER, COMMON, {}},
	{SYS_IPC_REPLY_TO, COMMON, {Arg::ID, Arg::VALUE, Arg::VALUE, Arg::VALUE, Arg::VALUE}},
	{SYS_SCHED_METRICS, RARE, {Arg::ADDRESS_OUT}},
};

/** Number of entries of `SPECS`. */
constexpr size_t SPEC_COUNT = sizeof(SPECS) / sizeof(SPECS[0]);

} // namespace fuzz

#endif // !FUZZ_SPECS_HPP
