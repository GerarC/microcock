#ifndef ARCH_X86_MULTIBOOT_HPP
#define ARCH_X86_MULTIBOOT_HPP

#include <stdint.h>
namespace cock::arch::x86 {

/**
 * @brief Multiboot AOUT symbol table structure.
 */
typedef struct _multiboot_aout_symbol_table_t {
	uint32_t tabsize;
	uint32_t strsize;
	uint32_t address;
	uint32_t reserved;
} MBAoutSymbolTable;

/**
 * @brief Multiboot ELF section header table structure.
 */
typedef struct _multiboot_elf_section_header_table_t {
	uint32_t num;
	uint32_t size;
	uint32_t address;
	uint32_t shndx;
} MBElfSectionHeaderTable;

/**
 * @brief The main Multiboot Information structure passed by the bootloader via
 * EBX.
 * * Contains flags indicating which fields are valid (memory maps, modules,
 * VBE, etc.).
 */
typedef struct _multiboot_info_t {
	uint32_t flags;		/**< Indicates which following fields are valid. */
	uint32_t mem_lower; /**< Amount of lower memory available. */
	uint32_t mem_upper; /**< Amount of upper memory available. */
	uint32_t
		boot_device;  /**< Indicates the device the kernel was booted from. */
	uint32_t cmdline; /**< Physical address of the command line string. */
	uint32_t mods_count; /**< Number of modules loaded by the bootloader. */
	uint32_t
		mods_address; /**< Physical address of the first module structure. */

	union {
		MBAoutSymbolTable aout_sym;
		MBElfSectionHeaderTable elf_section;
	} table;

	uint32_t mmap_length;  /**< Length of the memory map buffer. */
	uint32_t mmap_address; /**< Physical address of the memory map buffer. */

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

/**
 * @brief Represents a single entry in the Multiboot memory map.
 */
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
	uint32_t type; /**< Type of memory (Available, Reserved, ACPI, etc.). */
} MBMemoryMapEntry;

/**
 * @brief Structure representing a loaded boot module.
 */
typedef struct multiboot_mod_list {
	uint32_t mod_start; /**< Starting physical address of the module. */
	uint32_t mod_end;	/**< Ending physical address of the module. */
	uint32_t cmdline;	/**< Command line argument passed to the module. */
	uint32_t pad;
} MBModuleList;

} // namespace cock::arch::x86

#endif // !MULTIBOOT_HPP
