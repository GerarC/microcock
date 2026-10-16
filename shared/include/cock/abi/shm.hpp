#ifndef COCK_ABI_SHM_HPP
#define COCK_ABI_SHM_HPP

#include <stddef.h>

namespace cock::abi::shm {

constexpr size_t PAGE_BYTES = 0x1000;
constexpr size_t MAX_OBJECT_PAGES = 256;
constexpr size_t MAX_OBJECT_BYTES = MAX_OBJECT_PAGES * PAGE_BYTES;

} // namespace cock::abi::shm

#endif // !COCK_ABI_SHM_HPP
