/** @file
 * @brief The names the name server stores: paths, with `/run/` added to a short name.
 */
#ifndef NAMES_PATHS_HPP
#define NAMES_PATHS_HPP

#include <cock/abi/names.hpp>
#include <stddef.h>

namespace cock::names {

/** Longest stored name: a name of `MAX_NAME_BYTES` bytes with the `/run/` prefix. */
constexpr size_t MAX_PATH_BYTES = abi::names::MAX_NAME_BYTES + abi::names::RUN_PREFIX_BYTES;

/**
 * @brief Turns a name from the wire into the path the table stores.
 * @param name NUL terminated name of at most `abi::names::MAX_NAME_BYTES` bytes.
 * @param path Receives the path and a terminator. A name that starts with `/` is copied as it is; any other
 *        gets the prefix `/run/`; an empty name stays empty.
 */
void normalize(const char *name, char (&path)[MAX_PATH_BYTES + 1]);

} // namespace cock::names

#endif // !NAMES_PATHS_HPP
