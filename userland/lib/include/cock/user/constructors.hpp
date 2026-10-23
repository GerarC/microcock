#ifndef COCK_USER_CONSTRUCTORS_HPP
#define COCK_USER_CONSTRUCTORS_HPP

namespace cock::user {

/**
 * @brief Calls every static constructor of the program, in link order.
 *
 * The constructors are the function pointers between `__init_array_start` and
 * `__init_array_end`, which `userland/linker.ld` places around the `.init_array`
 * section. Called once by `_start` before `main`; calling it again runs them again.
 * A program with no global object that needs a constructor has an empty range and
 * nothing happens. It cannot fail.
 */
void runConstructors();

} // namespace cock::user

#endif
