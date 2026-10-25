/** @file
 * @brief The ends of the pipes: which process holds which end of which pipe.
 */
#ifndef PIPE_ENDS_HPP
#define PIPE_ENDS_HPP

#include <pipe/limits.hpp>
#include <pipe/pipes.hpp>
#include <stdint.h>

namespace cock::pipe {

/** @brief A read end or a write end held by one process. */
struct End {
	bool used; /**< True when the slot holds an end. */
	uint32_t owner; /**< Process id of the holder, the badge of its calls. */
	uint32_t handle; /**< Number of the end, local to the holder. */
	Pipe *pipe; /**< The pipe the end belongs to. */
	bool writer; /**< True for a write end. */
};

/**
 * @brief Finds an end of a process.
 * @param owner Process id of the caller.
 * @param handle The number the caller gave.
 * @return The end, or null when that process holds no end with that number.
 */
End *findEnd(uint32_t owner, uint32_t handle);

/**
 * @brief Makes a pipe and gives a process its read end and its write end.
 *
 * Ends of dead processes are closed first when the tables are full. Nothing changes on failure.
 * @param owner Process id of the caller.
 * @param readHandle Receives the number of the read end.
 * @param writeHandle Receives the number of the write end.
 * @return false when there is no room for a pipe or for two ends.
 */
bool createEnds(uint32_t owner, uint32_t &readHandle, uint32_t &writeHandle);

/**
 * @brief Closes an end, and frees the pipe when it was the last one.
 * @param end An end in use.
 */
void closeEnd(End &end);

/**
 * @brief Gives a copy made by `fork` the ends its parent holds, under the same numbers.
 *
 * Each end of the parent gets a twin held by the copy, on the same pipe, so a reader of the pipe sees the end of the
 * data only when both have closed theirs. Ends of dead processes are closed first when the table is full; when it
 * is still full the copy gets the ends that fit, and a call of the copy on another one is answered `BAD_HANDLE`.
 * Nothing is done for a copy that already holds an end.
 *
 * @param parent Process id of the process that was copied.
 * @param child Process id of the copy.
 */
void copyEnds(uint32_t parent, uint32_t child);

/**
 * @brief Closes every end of a process that no longer lives (`SYS_PROC_ALIVE`).
 * @return true when at least one end was closed.
 */
bool releaseDeadOwners();

/**
 * @brief Tells whether any end is open.
 * @return true when at least one slot is in use.
 */
bool anyEnd();

} // namespace cock::pipe

#endif // !PIPE_ENDS_HPP
