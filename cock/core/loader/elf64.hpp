/** @file
 * @brief ELF64 structures and constants for the loader.
 */
#ifndef CORE_LOADER_ELF64_HPP
#define CORE_LOADER_ELF64_HPP

#include <cock/core/loader/elf_common.hpp>
#include <hal/address_space.hpp>
#include <hal/executable.hpp>
#include <stdint.h>

namespace cock::core::loader {

/**
 * @brief Format traits and on-disk structures of the 64 bit ELF class.
 *
 * The loader is a template over this type (and Elf32), so supporting a class
 * costs nothing when the architecture does not use it.
 */
struct Elf64 {
	/** @brief Type wide enough to hold an address of this class. */
	using Address = uint64_t;

	/** @brief Value of the class byte in e_ident for this format. */
	static constexpr uint8_t CLASS_ID = 2;
	/** @brief True when the architecture can run this class. */
	static constexpr bool SUPPORTED = hal::ELF64_SUPPORTED;
	/** @brief Machine type (e_machine) accepted for this class. */
	static constexpr uint16_t MACHINE = hal::ELF64_MACHINE;
	/** @brief Upper bound of the user address space for this class. */
	static constexpr uint64_t USER_SPACE_LIMIT = hal::USER_SPACE_LIMIT_ELF64;

	/**
	 * @brief Byte offsets and sizes fixed by the ELF64 specification, used to
	 * check the structure definitions at compile time.
	 */
	struct Layout {
		/** @brief Size of the file header. */
		static constexpr size_t HEADER_SIZE = 64;
		/** @brief Offset of the entry point in the file header. */
		static constexpr size_t HEADER_ENTRY = 24;
		/** @brief Offset of the program header table offset field. */
		static constexpr size_t HEADER_PROGRAM_HEADER_OFFSET = 32;
		/** @brief Offset of the flags field in the file header. */
		static constexpr size_t HEADER_FLAGS = 48;
		/** @brief Offset of the program header entry size field. */
		static constexpr size_t HEADER_PROGRAM_HEADER_ENTRY_SIZE = 54;
		/** @brief Offset of the program header count field. */
		static constexpr size_t HEADER_PROGRAM_HEADER_NUM = 56;
		/** @brief Size of one program header. */
		static constexpr size_t PROGRAM_HEADER_SIZE = 56;
		/** @brief Offset of the flags field in a program header. */
		static constexpr size_t PROGRAM_HEADER_FLAGS = 4;
		/** @brief Offset of the file offset field in a program header. */
		static constexpr size_t PROGRAM_HEADER_OFFSET = 8;
		/** @brief Offset of the virtual address field in a program header. */
		static constexpr size_t PROGRAM_HEADER_VADDR = 16;
		/** @brief Offset of the size in file field in a program header. */
		static constexpr size_t PROGRAM_HEADER_SIZE_IN_FILE = 32;
		/** @brief Offset of the size in memory field in a program header. */
		static constexpr size_t PROGRAM_HEADER_SIZE_IN_MEMORY = 40;
	};

	/** @brief ELF64 file header, copied from the image before use. */
	struct Header {
		uint8_t ident[ELF_IDENT_SIZE]; /**< Magic, class, encoding, version. */
		uint16_t type; /**< Object file type, only executables are loaded. */
		uint16_t machine; /**< Target architecture. */
		uint32_t version; /**< Object file version. */
		uint64_t entry; /**< Virtual address of the entry point. */
		uint64_t program_header_offset; /**< File offset of the program header
										   table. */
		uint64_t section_header_offset; /**< File offset of the section header
										   table, ignored. */
		uint32_t flags; /**< Processor specific flags, ignored. */
		uint16_t header_size; /**< Size of this header. */
		uint16_t program_header_entry_size; /**< Size of one program header. */
		uint16_t program_header_num; /**< Number of program headers. */
		uint16_t section_header_entry_size; /**< Size of a section header,
											   ignored. */
		uint16_t section_header_num; /**< Number of section headers, ignored. */
		uint16_t string_table_index; /**< Section name table index, ignored. */
	};

	/** @brief ELF64 program header describing one segment. */
	struct ProgramHeader {
		uint32_t type; /**< Segment type, only loadable ones are used. */
		uint32_t flags; /**< Permission flags, such as ELF_PF_WRITE. */
		uint64_t offset; /**< Offset of the segment contents in the file. */
		uint64_t vaddr; /**< Virtual address where the segment is mapped. */
		uint64_t paddr; /**< Physical address, ignored. */
		uint64_t size_in_file; /**< Bytes to copy from the file. */
		uint64_t size_in_memory; /**< Bytes in memory, the rest is zero. */
		uint64_t align; /**< Alignment, ignored. */
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
