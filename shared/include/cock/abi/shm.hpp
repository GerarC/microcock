/**
 * @file shm.hpp
 * @brief Limits of shared memory objects (`SYS_SHM_*`).
 */
#ifndef COCK_ABI_SHM_HPP
#define COCK_ABI_SHM_HPP

#include <stddef.h>

namespace cock::abi::shm {

/** Size of one page in bytes. */
constexpr size_t PAGE_BYTES = 0x1000;
/** Most pages one shared object can have; larger requests fail. */
constexpr size_t MAX_OBJECT_PAGES = 1024;
/** Largest shared object in bytes (4 MiB). */
constexpr size_t MAX_OBJECT_BYTES = MAX_OBJECT_PAGES * PAGE_BYTES;

} // namespace cock::abi::shm

#endif // !COCK_ABI_SHM_HPP
