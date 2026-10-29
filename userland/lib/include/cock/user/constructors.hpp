#ifndef COCK_USER_CONSTRUCTORS_HPP
#define COCK_USER_CONSTRUCTORS_HPP

namespace cock::user {

/**
 * @brief Calls every static constructor of the program.
 *
 * A compiler puts the constructors in one of two sections, depending on how it was
 * configured. The function pointers between `__init_array_start` and `__init_array_end`
 * (the `.init_array` section) run first, in link order. Then the ones between
 * `__ctors_start` and `__ctors_end` (the older `.ctors` section) run from the last to
 * the first, which is the order that format defines. `userland/linker.ld` places the
 * symbols around both sections. Called once by `_start` before `main`; calling it again runs them again.
 * A program with no global object that needs a constructor has an empty range and
 * nothing happens. It cannot fail.
 */
void runConstructors();

} // namespace cock::user

#endif
