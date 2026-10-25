/** @file
 * @brief The policy `rs` carries in its code: what `ramfs` receives before the manifest is read.
 */
#ifndef RS_BOOT_HPP
#define RS_BOOT_HPP

#include <stdint.h>

namespace cock::rs {

/**
 * @brief Gives `ramfs` its label and the right to open the `initrd` boot module, tells the name
 * server which process it is, and tells `ramfs` so.
 *
 * Needs the name server to know `ramfs` already (`startNames`). Prints `rs: initrd is boot
 * module <n>`.
 *
 * @return The process id of `ramfs`, or `NO_PROCESS` when the `ramfs` process or the `initrd`
 * module is missing.
 */
uint32_t grantInitrd();

} // namespace cock::rs

#endif // !RS_BOOT_HPP
