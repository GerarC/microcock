/** @file
 * @brief What `rs` does besides telling the name server when `pm` made a copy with `fork`: it tells the programs
 * that keep state for each process, and then lets `pm` answer the parent.
 */
#ifndef RS_FORKS_HPP
#define RS_FORKS_HPP

#include <stdint.h>

namespace cock::rs {

/**
 * @brief Sends `PM_FORKED` to every running program whose manifest line has `notify=fork`.
 *
 * Each of them copies what the parent holds (open files, ends of pipes) for the copy. The message is queued in the
 * inbox of the program, and a program drains its inbox before it serves the next request, so it has copied before
 * the parent or the copy asks it anything. A program that does not run is skipped: it has nothing to copy.
 *
 * @param child Kernel process id of the copy.
 * @param parent Kernel process id of the process it copies.
 */
void forwardFork(uint32_t child, uint32_t parent);

/**
 * @brief Sends `PM_FORK_SETTLED` to `pm`, which then answers the parent of the copy.
 *
 * Called last, after `forwardFork` and after the name server knows the copy, so what the parent does when it
 * continues cannot change what the copy inherited. Nothing is sent when `pm` does not run.
 *
 * @param child Kernel process id of the copy.
 */
void acknowledgeFork(uint32_t child);

} // namespace cock::rs

#endif // !RS_FORKS_HPP
