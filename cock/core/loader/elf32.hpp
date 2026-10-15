#ifndef CORE_LOADER_ELF32_HPP
#define CORE_LOADER_ELF32_HPP

#include <cock/core/loader/elf_common.hpp>
#include <hal/address_space.hpp>
#include <hal/executable.hpp>
#include <stdint.h>

namespace cock::core::loader {

struct Elf32 {
	using Address = uint32_t;

	static constexpr uint8_t CLASS_ID = 1;
	static constexpr bool SUPPORTED = hal::ELF32_SUPPORTED;
	static constexpr uint16_t MACHINE = hal::ELF32_MACHINE;
	static constexpr uint64_t USER_SPACE_LIMIT = hal::USER_SPACE_LIMIT_ELF32;

	struct Layout {
		static constexpr size_t HEADER_SIZE = 52;
		static constexpr size_t HEADER_ENTRY = 24;
		static constexpr size_t HEADER_PROGRAM_HEADER_OFFSET = 28;
		static constexpr size_t HEADER_FLAGS = 36;
		static constexpr size_t HEADER_PROGRAM_HEADER_ENTRY_SIZE = 42;
		static constexpr size_t HEADER_PROGRAM_HEADER_NUM = 44;
		static constexpr size_t PROGRAM_HEADER_SIZE = 32;
		static constexpr size_t PROGRAM_HEADER_OFFSET = 4;
		static constexpr size_t PROGRAM_HEADER_VADDR = 8;
		static constexpr size_t PROGRAM_HEADER_SIZE_IN_FILE = 16;
		static constexpr size_t PROGRAM_HEADER_SIZE_IN_MEMORY = 20;
		static constexpr size_t PROGRAM_HEADER_FLAGS = 24;
	};

	struct Header {
		uint8_t ident[ELF_IDENT_SIZE];
		uint16_t type;
		uint16_t machine;
		uint32_t version;
		uint32_t entry;
		uint32_t program_header_offset;
		uint32_t section_header_offset;
		uint32_t flags;
		uint16_t header_size;
		uint16_t program_header_entry_size;
		uint16_t program_header_num;
		uint16_t section_header_entry_size;
		uint16_t section_header_num;
		uint16_t string_table_index;
	};

	struct ProgramHeader {
		uint32_t type;
		uint32_t offset;
		uint32_t vaddr;
		uint32_t paddr;
		uint32_t size_in_file;
		uint32_t size_in_memory;
		uint32_t flags;
		uint32_t align;
	};
};

static_assert(sizeof(Elf32::Header) == Elf32::Layout::HEADER_SIZE,
			  "ELF32 header size does not match the specification");
static_assert(sizeof(Elf32::ProgramHeader) ==
				  Elf32::Layout::PROGRAM_HEADER_SIZE,
			  "ELF32 program header size does not match the specification");
static_assert(
	offsetof(Elf32::Header, entry) == Elf32::Layout::HEADER_ENTRY &&
		offsetof(Elf32::Header, program_header_offset) ==
			Elf32::Layout::HEADER_PROGRAM_HEADER_OFFSET &&
		offsetof(Elf32::Header, flags) == Elf32::Layout::HEADER_FLAGS &&
		offsetof(Elf32::Header, program_header_entry_size) ==
			Elf32::Layout::HEADER_PROGRAM_HEADER_ENTRY_SIZE &&
		offsetof(Elf32::Header, program_header_num) ==
			Elf32::Layout::HEADER_PROGRAM_HEADER_NUM,
	"ELF32 header layout does not match the specification");
static_assert(
	offsetof(Elf32::ProgramHeader, offset) ==
			Elf32::Layout::PROGRAM_HEADER_OFFSET &&
		offsetof(Elf32::ProgramHeader, vaddr) ==
			Elf32::Layout::PROGRAM_HEADER_VADDR &&
		offsetof(Elf32::ProgramHeader, size_in_file) ==
			Elf32::Layout::PROGRAM_HEADER_SIZE_IN_FILE &&
		offsetof(Elf32::ProgramHeader, size_in_memory) ==
			Elf32::Layout::PROGRAM_HEADER_SIZE_IN_MEMORY &&
		offsetof(Elf32::ProgramHeader, flags) ==
			Elf32::Layout::PROGRAM_HEADER_FLAGS,
	"ELF32 program header layout does not match the specification");

} // namespace cock::core::loader

#endif // !CORE_LOADER_ELF32_HPP
