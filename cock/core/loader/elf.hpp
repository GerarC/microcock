#ifndef CORE_LOADER_ELF_HPP
#define CORE_LOADER_ELF_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::loader {

struct LoadedImage {
	uintptr_t entry = 0;
	uintptr_t user_space_limit = 0;
};

bool load_executable(const void *image, size_t size, uintptr_t address_space,
					 LoadedImage &out);

} // namespace cock::core::loader

#endif // !CORE_LOADER_ELF_HPP
