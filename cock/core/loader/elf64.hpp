#ifndef CORE_LOADER_ELF64_HPP
#define CORE_LOADER_ELF64_HPP

#include <cock/core/loader/elf_common.hpp>
#include <hal/address_space.hpp>
#include <hal/executable.hpp>
#include <stdint.h>

namespace cock::core::loader {

struct Elf64 {
	using Address = uint64_t;

	static constexpr uint8_t CLASS_ID = 2;
	static constexpr bool SUPPORTED = hal::ELF64_SUPPORTED;
	static constexpr uint16_t MACHINE = hal::ELF64_MACHINE;
	static constexpr uint64_t USER_SPACE_LIMIT = hal::USER_SPACE_LIMIT_ELF64;

	struct Layout {
		static constexpr size_t HEADER_SIZE = 64;
		static constexpr size_t HEADER_ENTRY = 24;
		static constexpr size_t HEADER_PROGRAM_HEADER_OFFSET = 32;
		static constexpr size_t HEADER_FLAGS = 48;
		static constexpr size_t HEADER_PROGRAM_HEADER_ENTRY_SIZE = 54;
		static constexpr size_t HEADER_PROGRAM_HEADER_NUM = 56;
		static constexpr size_t PROGRAM_HEADER_SIZE = 56;
		static constexpr size_t PROGRAM_HEADER_FLAGS = 4;
		static constexpr size_t PROGRAM_HEADER_OFFSET = 8;
		static constexpr size_t PROGRAM_HEADER_VADDR = 16;
		static constexpr size_t PROGRAM_HEADER_SIZE_IN_FILE = 32;
		static constexpr size_t PROGRAM_HEADER_SIZE_IN_MEMORY = 40;
	};

	struct Header {
		uint8_t ident[ELF_IDENT_SIZE];
		uint16_t type;
		uint16_t machine;
		uint32_t version;
		uint64_t entry;
		uint64_t program_header_offset;
		uint64_t section_header_offset;
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
		uint32_t flags;
		uint64_t offset;
		uint64_t vaddr;
		uint64_t paddr;
		uint64_t size_in_file;
		uint64_t size_in_memory;
		uint64_t align;
	};
};

static_assert(sizeof(Elf64::Header) == Elf64::Layout::HEADER_SIZE,
			  "ELF64 header size does not match the specification");
static_assert(sizeof(Elf64::ProgramHeader) ==
				  Elf64::Layout::PROGRAM_HEADER_SIZE,
			  "ELF64 program header size does not match the specification");
static_assert(
	offsetof(Elf64::Header, entry) == Elf64::Layout::HEADER_ENTRY &&
		offsetof(Elf64::Header, program_header_offset) ==
			Elf64::Layout::HEADER_PROGRAM_HEADER_OFFSET &&
		offsetof(Elf64::Header, flags) == Elf64::Layout::HEADER_FLAGS &&
		offsetof(Elf64::Header, program_header_entry_size) ==
			Elf64::Layout::HEADER_PROGRAM_HEADER_ENTRY_SIZE &&
		offsetof(Elf64::Header, program_header_num) ==
			Elf64::Layout::HEADER_PROGRAM_HEADER_NUM,
	"ELF64 header layout does not match the specification");
static_assert(
	offsetof(Elf64::ProgramHeader, flags) ==
			Elf64::Layout::PROGRAM_HEADER_FLAGS &&
		offsetof(Elf64::ProgramHeader, offset) ==
			Elf64::Layout::PROGRAM_HEADER_OFFSET &&
		offsetof(Elf64::ProgramHeader, vaddr) ==
			Elf64::Layout::PROGRAM_HEADER_VADDR &&
		offsetof(Elf64::ProgramHeader, size_in_file) ==
			Elf64::Layout::PROGRAM_HEADER_SIZE_IN_FILE &&
		offsetof(Elf64::ProgramHeader, size_in_memory) ==
			Elf64::Layout::PROGRAM_HEADER_SIZE_IN_MEMORY,
	"ELF64 program header layout does not match the specification");

} // namespace cock::core::loader

#endif // !CORE_LOADER_ELF64_HPP
