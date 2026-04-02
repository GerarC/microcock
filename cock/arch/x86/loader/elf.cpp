#include <cock/core/hal/tasking.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/utils/panic.hpp>
#include <cock/core/boot/module.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::core::hal {

static constexpr size_t MAGIC_NUMBER_SIZE = 0x10;
static constexpr size_t PATTERN_SIZE = 0x4;
static constexpr uint8_t MAGIC_NUMBER_PATTERN[PATTERN_SIZE] = {0x7F, 'E', 'L',
															   'F'};
static constexpr uint32_t PT_LOAD = 0x01;
static constexpr size_t PAGE_SIZE = 0x1000;
static constexpr uint32_t CEIL_ALIGNMENT = 0xFFF;

typedef struct Elf32_Ehdr {
	uint8_t ident[MAGIC_NUMBER_SIZE];
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
} Elf32Header;

typedef struct Elf32_Phdr {
	uint32_t type;
	uint32_t offset;
	uint32_t vaddr;
	uint32_t paddr;
	uint32_t program_header_size;
	uint32_t size_in_memory;
	uint32_t flags;
	uint32_t align;
} Elf32ProgramHeader;

uintptr_t load_executable(void *file_in_mem, uintptr_t address_space) {
	Elf32Header *header = reinterpret_cast<Elf32Header *>(file_in_mem);

	for (size_t idx = 0; idx < PATTERN_SIZE; idx++) {
		if (header->ident[idx] != MAGIC_NUMBER_PATTERN[idx])
			utils::panic("Is not a Elf file");
	}

	uintptr_t old_cr3 = get_current_address_space();
	switch_address_space(address_space);

	Elf32ProgramHeader *program_headers =
		reinterpret_cast<Elf32ProgramHeader *>(
			reinterpret_cast<uint8_t *>(file_in_mem) +
			header->program_header_offset);
	for (size_t idx = 0; idx < header->program_header_num; idx++) {
		if (program_headers[idx].type == PT_LOAD) {
			size_t needed_pages =
				(program_headers[idx].size_in_memory + CEIL_ALIGNMENT) /
				PAGE_SIZE;
			memory::VirtualMemoryManager::mapPagesAt(
				program_headers[idx].vaddr, needed_pages,
				memory::VMMPermission::USER_DATA);

			void *dest = reinterpret_cast<void *>(program_headers[idx].vaddr);
			void *source = reinterpret_cast<void *>(
				reinterpret_cast<uint8_t *>(file_in_mem) +
				program_headers[idx].offset);
			memset(dest, 0, program_headers[idx].size_in_memory);
			memcpy(dest, source, program_headers[idx].program_header_size);
		}
	}

	switch_address_space(old_cr3);
	return static_cast<uintptr_t>(header->entry);
}

} // namespace cock::core::hal
