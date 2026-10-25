/** @file
 * @brief The shared memory objects `pm` made for callers that will ask for a process.
 */
#ifndef PM_OBJECTS_HPP
#define PM_OBJECTS_HPP

#include <cock/abi/pm.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::pm {

/** Most callers that hold an object at once; each caller holds one. */
constexpr size_t MAX_OBJECTS = 4;
/** Object id of a free slot. */
constexpr uint32_t NO_OBJECT = 0xFFFFFFFF;

/** An object that `pm` made and shared with one caller. */
struct Object {
	/** Kernel process id of the caller, the badge of its handle; `NO_OBJECT` marks a free slot in `id`. */
	uint32_t caller;
	/** Id of the shared object, or `NO_OBJECT`. */
	uint32_t id;
	/** Size in bytes, a whole number of pages; `pm` reads nothing past it. */
	size_t bytes;
};

/**
 * @brief Makes a shared object for a caller and shares it with it for writing.
 *
 * A caller that already holds one gets its old object released first, so a caller cannot take more than one slot.
 *
 * @param caller Kernel process id of the caller.
 * @param bytes Size asked for, from 1 to `shm::MAX_OBJECT_BYTES`; the object is rounded up to whole pages.
 * @param id Receives the object id.
 * @return `OK`; `BAD_REQUEST` for a size out of range; `NO_ROOM` when every slot belongs to another caller or the
 * kernel has no memory for the object; `FAILED` when the kernel refused to share it with the caller.
 */
cock::abi::pm::Status allocateObject(uint32_t caller, uint32_t bytes, uint32_t &id);

/**
 * @brief Finds the object a caller asked for.
 * @param caller Kernel process id of the caller.
 * @param id Object id the caller names.
 * @return The slot, or null when the caller holds no object with that id.
 */
const Object *findObject(uint32_t caller, uint32_t id);

/**
 * @brief Gives the object of a caller back to the kernel.
 * @param caller Kernel process id of the caller; nothing happens when it holds none.
 */
void dropObject(uint32_t caller);

} // namespace cock::pm

#endif // !PM_OBJECTS_HPP
