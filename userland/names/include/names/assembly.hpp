/** @file
 * @brief Putting together a name that arrives in parts, one synchronous call each.
 */
#ifndef NAMES_ASSEMBLY_HPP
#define NAMES_ASSEMBLY_HPP

#include <cock/abi/names.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::names {

/** Names being put together at the same time, one per calling thread. */
constexpr size_t MAX_ASSEMBLIES = 8;

/** Result of adding one part. */
enum class Assembled : uint32_t {
	/** The part was kept and more are expected. */
	MORE,
	/** The last part arrived; the whole name is ready. */
	DONE,
	/** The part is not acceptable; whatever was kept for the caller is dropped. */
	BAD,
};

/** Tells whether a thread is alive. */
using AliveFunction = bool (*)(uint32_t);

/**
 * @brief Adds one part of a name.
 *
 * Part 0 starts a name (and drops an unfinished one of the same caller). Every other part must be the one
 * after the last that arrived from that caller, and every part but the last must fill its 12 bytes. A caller
 * that stops half way leaves a slot that is reclaimed, when the table is full, if the thread ended.
 * @param caller Id of the calling thread, as the kernel stamped it.
 * @param header Header word of the request.
 * @param words The request words; words 1 to 3 hold the bytes.
 * @param alive Tells whether a thread still runs, to reclaim the slots of the ones that ended.
 * @param allowEmpty True when a name with no bytes is acceptable, which `DEFINE` uses for a program that
 *        cannot be reached by name.
 * @param name Receives the whole name and a terminator when the result is `DONE`.
 * @return `MORE`, `DONE` or `BAD` (a part out of order, a name that grows past `MAX_NAME_BYTES`, an empty name
 *         when it is not allowed, or no free slot).
 */
Assembled addPart(uint32_t caller, uint32_t header,
				  const uint32_t (&words)[abi::names::CALL_WORDS], AliveFunction alive,
				  bool allowEmpty, char (&name)[abi::names::MAX_NAME_BYTES + 1]);

} // namespace cock::names

#endif // !NAMES_ASSEMBLY_HPP
