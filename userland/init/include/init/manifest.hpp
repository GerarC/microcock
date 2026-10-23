/** @file
 * @brief The manifest of programs and capabilities, read from the initrd and parsed.
 */
#ifndef INIT_MANIFEST_HPP
#define INIT_MANIFEST_HPP

#include <cock/abi/manifest.hpp>
#include <cock/user/fs.hpp>

namespace cock::init {

/** @brief The parsed manifest. Valid after `loadManifest` succeeds. */
extern cock::abi::manifest::Manifest manifest;

/**
 * @brief Connects to the file server, reads `manifest.conf` and parses it.
 *
 * Prints `init: cannot read` when the server cannot be reached or the file cannot be read, a
 * line with the error and its line number when the parser rejects it, and the number of
 * entries on success.
 *
 * @param client Receives the connection, which later reads the program images.
 * @return True when `manifest` holds the entries, false on any failure.
 */
bool loadManifest(cock::user::fs::Client &client);

} // namespace cock::init

#endif // !INIT_MANIFEST_HPP
