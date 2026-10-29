#ifndef ARCH_X86_MULTIBOOT_HPP
#define ARCH_X86_MULTIBOOT_HPP

/**
 * @file
 * @brief Multiboot (version 1) information structures handed over by GRUB.
 *
 * GRUB enters the kernel with EAX equal to MULTIBOOT_BOOTLOADER_MAGIC and EBX
 * holding the physical address of an MBInfo. All addresses inside these
 * structures are physical and may be above the directly mapped area: reach them
 * through BootMemory::map(). Every field is untrusted until its flag in
 * MBInfo::flags has been checked.
 */

#include <stdint.h>
namespace cock::arch::x86 {

/** @brief Value GRUB leaves in EAX to prove a Multiboot loader started us. */
constexpr uint32_t MULTIBOOT_BOOTLOADER_MAGIC = 0x2BADB002;
/** @brief MBInfo::flags bit 0: mem_lower and mem_upper are valid. */
constexpr uint32_t MULTIBOOT_FLAG_MEMORY = 1u << 0;
/** @brief MBInfo::flags bit 2: cmdline is valid. */
constexpr uint32_t MULTIBOOT_FLAG_CMDLINE = 1u << 2;
/** @brief MBInfo::flags bit 3: mods_count and mods_address are valid. */
constexpr uint32_t MULTIBOOT_FLAG_MODULES = 1u << 3;
/** @brief MBInfo::flags bit 6: mmap_length and mmap_address are valid. */
constexpr uint32_t MULTIBOOT_FLAG_MMAP = 1u << 6;

/**
 * @brief Multiboot AOUT symbol table structure.
 */
typedef struct _multiboot_aout_symbol_table_t {
	uint32_t tabsize;  /**< Size in bytes of the symbol table. */
	uint32_t strsize;  /**< Size in bytes of the string table. */
	uint32_t address;  /**< Physical address of the symbol table. */
	uint32_t reserved; /**< Reserved by the specification, must be ignored. */
} MBAoutSymbolTable;

/**
 * @brief Multiboot ELF section header table structure.
 */
typedef struct _multiboot_elf_section_header_table_t {
	uint32_t num;	  /**< Number of section headers. */
	uint32_t size;	  /**< Size in bytes of each section header. */
	uint32_t address; /**< Physical address of the section header table. */
	uint32_t shndx;	  /**< Index of the section name string table. */
} MBElfSectionHeaderTable;

/**
 * @brief The main Multiboot Information structure passed by the bootloader via
 * EBX.
 * * Contains flags indicating which fields are valid (memory maps, modules,
 * VBE, etc.).
 *
 * Lives in memory owned by the bootloader: the physical memory manager
 * reserves it so it is not recycled before it has been consumed.
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

	/** @brief Kernel symbol information; which member applies depends on the
	 * kernel image format (flags bits 4 and 5). */
	union {
		MBAoutSymbolTable aout_sym;		   /**< Valid for a.out kernels. */
		MBElfSectionHeaderTable elf_section; /**< Valid for ELF kernels. */
	} table;

	uint32_t mmap_length;  /**< Length of the memory map buffer. */
	uint32_t mmap_address; /**< Physical address of the memory map buffer. */

	uint32_t config_table;	  /**< Physical address of the BIOS drives table. */
	uint32_t boot_loader_name; /**< Physical address of the loader name string. */

	uint32_t apm_table; /**< Physical address of the APM table. */

	uint32_t vbe_control_info; /**< Physical address of the VBE control info. */
	uint32_t vbe_mode_info;	   /**< Physical address of the VBE mode info. */
	uint16_t vbe_mode;		   /**< Current VBE mode. */
	uint16_t vbe_interface_seg; /**< VBE 2.0 protected mode interface segment. */
	uint16_t vbe_interface_off; /**< VBE 2.0 protected mode interface offset. */
	uint16_t vbe_interface_len; /**< VBE 2.0 protected mode interface length. */

} MBInfo;

/**
 * @brief Represents a single entry in the Multiboot memory map.
 *
 * Entries are packed and variable stride: the next entry starts at
 * `size + 4` bytes after the current one, not at sizeof(MBMemoryMapEntry).
 * The field name `lenght_*` keeps its historical spelling.
 */
typedef struct __attribute__((packed)) _multiboot_mmap_entry_t {
	uint32_t size;		/**< Size of the entry not counting this field. */
	uint32_t address_low;  /**< Low 32 bits of the region base address. */
	uint32_t address_high; /**< High 32 bits of the region base address. */
	uint32_t lenght_low;   /**< Low 32 bits of the region length in bytes. */
	uint32_t lenght_high;  /**< High 32 bits of the region length in bytes. */
/** @brief Memory map type: usable RAM. */
#define MULTIBOOT_MEMORY_AVAILABLE 1
/** @brief Memory map type: reserved, not usable. */
#define MULTIBOOT_MEMORY_RESERVED 2
/** @brief Memory map type: ACPI tables that can be reclaimed after parsing. */
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
/** @brief Memory map type: ACPI non-volatile storage, must be preserved. */
#define MULTIBOOT_MEMORY_NVS 4
/** @brief Memory map type: defective RAM, must not be used. */
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
	uint32_t pad;		/**< Padding, must be ignored. */
} MBModuleList;

} // namespace cock::arch::x86

#endif // !MULTIBOOT_HPP
