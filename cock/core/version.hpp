/** @file
 * @brief Kernel version and build identification, overridable from the build
 * system with preprocessor definitions.
 */
#ifndef COCK_VERSION_HPP
#define COCK_VERSION_HPP

/** @brief Major version number, defined by the build or 1 by default. */
#ifndef COCK_VERSION_MAJOR
#define COCK_VERSION_MAJOR 1
#endif

/** @brief Minor version number, defined by the build or 0 by default. */
#ifndef COCK_VERSION_MINOR
#define COCK_VERSION_MINOR 0
#endif

/** @brief Patch version number, defined by the build or 0 by default. */
#ifndef COCK_VERSION_PATCH
#define COCK_VERSION_PATCH 0
#endif

/** @brief Release stage text, such as dev. */
#ifndef COCK_VERSION_STAGE
#define COCK_VERSION_STAGE "dev"
#endif

/** @brief Git commit hash of the build, or unknown. */
#ifndef COCK_GIT_HASH
#define COCK_GIT_HASH "unknown"
#endif

/** @brief Build date text, or unknown. */
#ifndef COCK_BUILD_DATE
#define COCK_BUILD_DATE "unknown"
#endif

/** @brief Turns its argument into a string literal without expanding it. */
#define _STR_HELPER(x) #x
/** @brief Expands a macro argument and turns the result into a string literal. */
#define STR(x) _STR_HELPER(x)

/** @brief Banner such as microcock v1.0.0 dev, built from the version macros. */
#define COCK_VERSION_STRING \
    "microcock v" STR(COCK_VERSION_MAJOR) "." STR(COCK_VERSION_MINOR) "." STR(COCK_VERSION_PATCH) " " COCK_VERSION_STAGE

namespace cock::core {
/** @brief Major version as an integer. */
constexpr int VERSION_MAJOR = COCK_VERSION_MAJOR;
/** @brief Minor version as an integer. */
constexpr int VERSION_MINOR = COCK_VERSION_MINOR;
/** @brief Patch version as an integer. */
constexpr int VERSION_PATCH = COCK_VERSION_PATCH;
/** @brief Release stage text. */
constexpr const char* VERSION_STAGE = COCK_VERSION_STAGE;
/** @brief Complete version banner. */
constexpr const char* VERSION_STRING = COCK_VERSION_STRING;
}

#endif // !COCK_VERSION_HPP
