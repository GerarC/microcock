#include <cock/core/loader/elf.hpp>
#include <cock/core/loader/elf32.hpp>
#include <cock/core/loader/elf64.hpp>
#include <cock/core/loader/elf_common.hpp>
#include <cock/core/loader/elf_loader.hpp>
#include <cock/utils/logger.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::core::loader {

using utils::Logger;

namespace {

template <typename Format>
bool try_load(const uint8_t *base, size_t size, memory::PhysAddr address_space,
			  LoadedImage &out) {
	if constexpr (Format::SUPPORTED) {
		return detail::load_image<Format>(base, size, address_space, out);
	} else {
		(void)base;
		(void)size;
		(void)address_space;
		(void)out;
		Logger::error("ELF: class %u is not supported by this architecture",
					  static_cast<unsigned>(Format::CLASS_ID));
		return false;
	}
}

} // namespace

bool load_executable(const void *image, size_t size,
					 memory::PhysAddr address_space, LoadedImage &out) {
	out = LoadedImage{};
	if (!image || address_space.isNull() || size < ELF_IDENT_SIZE) return false;

	const uint8_t *base = static_cast<const uint8_t *>(image);
	if (memcmp(base, ELF_MAGIC, ELF_MAGIC_SIZE) != 0) {
		Logger::error("ELF: bad magic number");
		return false;
	}

	switch (base[ELF_IDENT_CLASS]) {
		case Elf32::CLASS_ID:
			return try_load<Elf32>(base, size, address_space, out);
		case Elf64::CLASS_ID:
			return try_load<Elf64>(base, size, address_space, out);
		default:
			Logger::error("ELF: unknown class %u",
						  static_cast<unsigned>(base[ELF_IDENT_CLASS]));
			return false;
	}
}

} // namespace cock::core::loader
