#ifndef COCK_VERSION_HPP
#define COCK_VERSION_HPP

#ifndef COCK_VERSION_MAJOR
#define COCK_VERSION_MAJOR 1
#endif

#ifndef COCK_VERSION_MINOR
#define COCK_VERSION_MINOR 0
#endif

#ifndef COCK_VERSION_PATCH
#define COCK_VERSION_PATCH 0
#endif

#ifndef COCK_VERSION_STAGE
#define COCK_VERSION_STAGE "dev"
#endif

#ifndef COCK_GIT_HASH
#define COCK_GIT_HASH "unknown"
#endif

#ifndef COCK_BUILD_DATE
#define COCK_BUILD_DATE "unknown"
#endif

#define _STR_HELPER(x) #x
#define STR(x) _STR_HELPER(x)

#define COCK_VERSION_STRING \
    "microcock v" STR(COCK_VERSION_MAJOR) "." STR(COCK_VERSION_MINOR) "." STR(COCK_VERSION_PATCH) " " COCK_VERSION_STAGE

namespace cock::core {
constexpr int VERSION_MAJOR = COCK_VERSION_MAJOR;
constexpr int VERSION_MINOR = COCK_VERSION_MINOR;
constexpr int VERSION_PATCH = COCK_VERSION_PATCH;
constexpr const char* VERSION_STAGE = COCK_VERSION_STAGE;
constexpr const char* VERSION_STRING = COCK_VERSION_STRING;
}

#endif // !COCK_VERSION_HPP

