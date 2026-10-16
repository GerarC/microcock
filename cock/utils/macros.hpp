/** @file
 * @brief Small utility macros shared by the kernel.
 */
#ifndef COCK_UTILS_MACROS_HPP
#define COCK_UTILS_MACROS_HPP

/** @brief Starts an infinite loop, used where execution must never continue,
 * such as after a halt. */
#define FOR_ETERNAL for (;;)

#endif // !COCK_UTILS_MACROS_HPP
