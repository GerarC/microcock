/**
 * @file cxa_constructors.hpp
 * @brief Kernel start-up hook that runs the global constructors.
 */
#ifndef CXA_CONSTRUCTORS_HPP
#define CXA_CONSTRUCTORS_HPP

/**
 * @brief Calls every global constructor of the kernel image.
 *
 * Walks the `.init_array` table, or the `.ctors` table when the first is
 * empty. Entries that do not point into the kernel text (between the kernel
 * space base and the end of the kernel) are skipped. Must be called once,
 * early in `kernel_main`, after the heap exists and before any global object
 * is used.
 */
extern "C" void call_global_constructors();

#endif // !CXA_CONSTRUCTORS_HPP
