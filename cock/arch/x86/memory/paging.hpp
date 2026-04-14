#ifndef PAGING_HPP
#define PAGING_HPP
#include <stdint.h>

namespace cock::arch::x86 {

constexpr uint32_t PAGE_TABLE_ENTRIES = 0x400;
constexpr uint32_t PAGE_DIRECTORY_ENTRIES = 0x400;
constexpr uint32_t PAGE_TABLE_ALIGNMENT = 0x1000;
constexpr uint32_t PAGE_DIRECTORY_ALIGNMENT = 0x1000;

/**
 * @brief Represents the hardware flags available in an x86 Page Table Entry
 * (PTE).
 */
enum PageFlag : uintptr_t {
	PRESENT = 1 << 0, /**< Page is present in physical memory. */
	WRITEABLE =
		1 << 1,		   /**< Page is read/write (if clear, page is read-only). */
	USER = 1 << 2,	   /**< Page is accessible from Ring 3 (Userland). */
	PWT = 1 << 3,	   /**< Page-level write-through. */
	PCD = 1 << 4,	   /**< Page-level cache disable. */
	ACCESSED = 1 << 5, /**< Set by the CPU when the page is accessed. */
	DIRTY = 1 << 6,	   /**< Set by the CPU when the page is written to. */
	HUGE =
		1 << 7, /**< Indicates a 4MB page if set in a Page Directory Entry. */
};

typedef uint32_t PageTableEntry;
typedef uint32_t PageDirectoryEntry;

/**
 * @brief Hardware-defined x86 Page Table.
 * * Contains 1024 Page Table Entries mapping 4KB frames each. Must be 4KB
 * aligned.
 */
typedef struct page_table_t {
	PageTableEntry entries[PAGE_TABLE_ENTRIES];
} __attribute__((aligned(PAGE_TABLE_ALIGNMENT))) PageTable;

/**
 * @brief Hardware-defined x86 Page Directory.
 * * Contains 1024 Page Directory Entries pointing to Page Tables. Must be 4KB
 * aligned.
 */
typedef struct page_directory_t {
	PageTableEntry entries[PAGE_DIRECTORY_ENTRIES];
} __attribute__((aligned(PAGE_DIRECTORY_ALIGNMENT))) PageDirectory;

} // namespace cock::arch::x86

#endif // !PAGING_HPP
