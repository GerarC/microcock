/** @file
 * @brief The manifest as `init` reads it: only to find the programs it must start.
 */
#ifndef INIT_MANIFEST_HPP
#define INIT_MANIFEST_HPP

#include <cock/abi/manifest.hpp>
#include <cock/user/fs.hpp>

namespace cock::init {

/** The parsed manifest. Valid after `loadManifest` succeeds. */
extern cock::abi::manifest::Manifest manifest;

/**
 * @brief Reads `/manifest.conf` through the file server and parses it.
 *
 * `rs` read the same file to start the system; `init` reads it again to learn which entries carry `parent=init`
 * and at which position, since the position gives the service label (`abi::service::programLabel`). Prints
 * `init: cannot read` when the file cannot be read and a line with the error and its number when the parser
 * rejects it.
 *
 * @param files A connected file client.
 * @return True when `manifest` holds the entries.
 */
bool loadManifest(cock::user::fs::Client &files);

} // namespace cock::init

#endif // !INIT_MANIFEST_HPP
