#ifndef MULTIBOOT_HPP
#define MULTIBOOT_HPP

#include <stdint.h>
namespace cock::arch::x86 {

typedef struct _multiboot_aout_symbol_table_t {
	uint32_t tabsize;
	uint32_t strsize;
	uint32_t address;
	uint32_t reserved;
} MBAoutSymbolTable;

typedef struct _multiboot_elf_section_header_table_t {
	uint32_t num;
	uint32_t size;
	uint32_t address;
	uint32_t shndx;
} MBElfSectionHeaderTable;

typedef struct _multiboot_info_t {
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_address;

	union {
		MBAoutSymbolTable aout_sym;
		MBElfSectionHeaderTable elf_section;
	} table;

	uint32_t mmap_length;
	uint32_t mmap_address;

	uint32_t config_table;
	uint32_t boot_loader_name;

	uint32_t apm_table;

	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;

} MBInfo;

typedef struct __attribute__((packed)) _multiboot_mmap_entry_t {
	uint32_t size;
	uint32_t address_low;
	uint32_t address_high;
	uint32_t lenght_low;
	uint32_t lenght_high;
#define MULTIBOOT_MEMORY_AVAILABLE 1
#define MULTIBOOT_MEMORY_RESERVED 2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS 4
#define MULTIBOOT_MEMORY_BADRAM 5
	uint32_t type;
} MBMemoryMapEntry;

} // namespace cock::arch::x86

#endif // !MULTIBOOT_HPP
