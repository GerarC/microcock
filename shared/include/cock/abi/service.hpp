/**
 * @file service.hpp
 * @brief Service labels: the stable names under which the IPC capability authorizes communication.
 *
 * A label belongs to a program, not to a running process, so it survives a restart: `init` gives the
 * same label to the new process that replaces a dead server, and every capability held for that
 * label keeps working. A process has no label until `SYS_SERVICE_SET` gives it one, and the label
 * can be set once.
 */
#ifndef SHARED_COCK_ABI_SERVICE_HPP
#define SHARED_COCK_ABI_SERVICE_HPP

#include <stdint.h>

namespace cock::abi::service {

/** Label of a process that has none. No capability covers it except the root one. */
constexpr uint32_t NO_SERVICE = 0;
/** Label that the kernel gives to the first program, `init`. */
constexpr uint32_t ROOT_SERVICE = 1;
/** Label that `init` gives to the `ramfs` boot module, which has no line in the manifest. */
constexpr uint32_t RAMFS_SERVICE = 2;
/** Label of the first program of the manifest; the program at index `n` gets `PROGRAM_SERVICE_BASE + n`. */
constexpr uint32_t PROGRAM_SERVICE_BASE = 16;
/** Number of labels that the root IPC capability covers, from `NO_SERVICE` on: all of them. */
constexpr uint64_t SERVICE_LABEL_COUNT = uint64_t(1) << 32;

} // namespace cock::abi::service

#endif // !SHARED_COCK_ABI_SERVICE_HPP
