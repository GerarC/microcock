/**
 * @file classify.hpp
 * @brief Turns the states of the live threads of a process, as `SYS_PROC_STATE` returns them, into the state
 * of its row in `ps`.
 */
#ifndef COCK_PM_CLASSIFY_HPP
#define COCK_PM_CLASSIFY_HPP

#include <cock/abi/pm.hpp>
#include <cock/abi/process.hpp>
#include <stdint.h>

namespace cock::pm {

/**
 * @brief Picks the state of a process from the states of its live threads.
 *
 * A stopped process is `STOPPED` whatever its threads wait for. Otherwise one runnable thread is enough for
 * `RUNNING`, because the process can use the CPU; the process sleeps only when every live thread waits. Bits that
 * are not `THREAD_*` bits are ignored.
 *
 * @param mask Mask of `abi::process::THREAD_*` bits, as `SYS_PROC_STATE` returns it.
 * @return `STOPPED` when `THREAD_STOPPED` is set, `RUNNING` when a thread is ready or running, `SLEEPING` when
 * only waits are set, and `FREE` when the mask holds no known bit: the caller then keeps the state it already has.
 */
inline abi::pm::State classify(uint32_t mask) {
	if ((mask & abi::process::THREAD_STOPPED) != 0) return abi::pm::State::STOPPED;
	if ((mask & abi::process::THREAD_RUNNABLE_MASK) != 0) return abi::pm::State::RUNNING;
	if ((mask & abi::process::THREAD_SLEEPING_MASK) != 0) return abi::pm::State::SLEEPING;
	return abi::pm::State::FREE;
}

} // namespace cock::pm

#endif // !COCK_PM_CLASSIFY_HPP
