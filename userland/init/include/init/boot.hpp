/** @file
 * @brief The policy `init` carries in its code: what `ramfs` receives before the manifest is read.
 */
#ifndef INIT_BOOT_HPP
#define INIT_BOOT_HPP

namespace cock::init {

/**
 * @brief Gives `ramfs` the right to open the `initrd` boot module and tells it so.
 *
 * Prints `init: initrd is boot module <n>`.
 *
 * @return True when the `ramfs` process and the `initrd` module exist and the grant was made,
 * false when either is missing.
 */
bool grantInitrd();

} // namespace cock::init

#endif // !INIT_BOOT_HPP
