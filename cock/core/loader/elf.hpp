#ifndef CORE_LOADER_ELF_HPP
#define CORE_LOADER_ELF_HPP

#include <cock/core/memory/address.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::loader {

struct LoadedImage {
	memory::VirtAddr entry;
	memory::VirtAddr user_space_limit;
};

bool load_executable(const void *image, size_t size,
					 memory::PhysAddr address_space, LoadedImage &out);

} // namespace cock::core::loader

#endif // !CORE_LOADER_ELF_HPP
