/** @file
 * @brief The manifest of programs and capabilities, read from the initrd and parsed.
 */
#ifndef RS_MANIFEST_HPP
#define RS_MANIFEST_HPP

#include <cock/abi/manifest.hpp>
#include <cock/user/fs.hpp>
#include <stdint.h>

namespace cock::rs {

/** @brief The parsed manifest. Valid after `loadManifest` succeeds. */
extern cock::abi::manifest::Manifest manifest;

/**
 * @brief Connects to the file server, reads `manifest.conf` and parses it.
 *
 * Prints `rs: cannot read` when the server cannot be reached or the file cannot be read, a
 * line with the error and its line number when the parser rejects it, and the number of
 * entries on success.
 *
 * @param client Receives the connection, which later reads the program images.
 * @param files Process id of `ramfs`, which `rs` reaches without the name server, through a handle it
 * makes for itself (`handleOf`).
 * @return True when `manifest` holds the entries, false on any failure.
 */
bool loadManifest(cock::user::fs::Client &client, uint32_t files);

} // namespace cock::rs

#endif // !RS_MANIFEST_HPP
