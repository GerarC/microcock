/** @file
 * @brief The version of the system, from the `UNAME_VERSION_*` flags of the build.
 */
#ifndef UNAME_VERSION_HPP
#define UNAME_VERSION_HPP

namespace cock::uname {

/** @brief Value of a version part when the build does not pass one. */
constexpr unsigned NO_VERSION = 0;

#ifdef UNAME_VERSION_MAJOR
/** @brief Major version of the system. */
constexpr unsigned VERSION_MAJOR = UNAME_VERSION_MAJOR;
/** @brief Minor version of the system. */
constexpr unsigned VERSION_MINOR = UNAME_VERSION_MINOR;
/** @brief Patch version of the system. */
constexpr unsigned VERSION_PATCH = UNAME_VERSION_PATCH;
#else
/** @brief Major version of the system. */
constexpr unsigned VERSION_MAJOR = NO_VERSION;
/** @brief Minor version of the system. */
constexpr unsigned VERSION_MINOR = NO_VERSION;
/** @brief Patch version of the system. */
constexpr unsigned VERSION_PATCH = NO_VERSION;
#endif

} // namespace cock::uname

#endif // !UNAME_VERSION_HPP
