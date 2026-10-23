/** @file
 * @brief The version the shell prints, taken from the root `Makefile`.
 *
 * The `Makefile` passes `SH_VERSION_MAJOR`, `SH_VERSION_MINOR` and `SH_VERSION_PATCH`. When
 * they are not defined, every part is zero.
 */
#ifndef SH_VERSION_HPP
#define SH_VERSION_HPP

namespace cock::sh {

/** @brief Value of a version part when the build does not pass one. */
constexpr unsigned NO_VERSION = 0;

#ifdef SH_VERSION_MAJOR
/** @brief Major version of the system. */
constexpr unsigned VERSION_MAJOR = SH_VERSION_MAJOR;
/** @brief Minor version of the system. */
constexpr unsigned VERSION_MINOR = SH_VERSION_MINOR;
/** @brief Patch version of the system. */
constexpr unsigned VERSION_PATCH = SH_VERSION_PATCH;
#else
/** @brief Major version of the system. */
constexpr unsigned VERSION_MAJOR = NO_VERSION;
/** @brief Minor version of the system. */
constexpr unsigned VERSION_MINOR = NO_VERSION;
/** @brief Patch version of the system. */
constexpr unsigned VERSION_PATCH = NO_VERSION;
#endif

} // namespace cock::sh

#endif // !SH_VERSION_HPP
