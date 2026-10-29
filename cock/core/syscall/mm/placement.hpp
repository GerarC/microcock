/** @file
 * @brief Choice of user virtual addresses for mappings the kernel places
 * (anonymous memory and shared memory).
 */
#ifndef CORE_SYSCALL_PLACEMENT_HPP
#define CORE_SYSCALL_PLACEMENT_HPP

#include <cock/core/memory/address.hpp>
#include <cock/core/task/thread.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::syscall {

/**
 * @brief Upper bound for program images and mappings of a thread.
 * @param thread Thread whose process limit applies.
 * @return The user space limit minus the user stack, as a raw address.
 */
uintptr_t user_image_limit(const task::Thread &thread);

/**
 * @brief Finds a free range of user pages for a new mapping.
 *
 * Searches from the start of the mmap area up to the image limit, skipping
 * ranges that are recorded as regions or already mapped by other means (for
 * instance the program image). Gives up after a bounded number of attempts.
 *
 * @param current Thread asking, its process is searched.
 * @param pages Size in pages, non zero.
 * @return The start of a free range, or a null address when none was found.
 */
memory::VirtAddr find_user_placement(task::Thread &current, size_t pages);

} // namespace cock::core::syscall

#endif // !CORE_SYSCALL_PLACEMENT_HPP
