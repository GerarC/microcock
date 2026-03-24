#ifndef PAGING_HPP
#define PAGING_HPP
#include <stdint.h>

namespace cock::arch::x86 {

constexpr uint32_t PAGE_TABLE_ENTRIES =  0x400;
constexpr uint32_t PAGE_DIRECTORY_ENTRIES =  0x400;
constexpr uint32_t PAGE_TABLE_ALIGNMENT = 0x1000;
constexpr uint32_t PAGE_DIRECTORY_ALIGNMENT = 0x1000;

enum PageFlag : uintptr_t {
	PRESENT     = 1 << 0,
	WRITEABLE   = 1 << 1,
	USER        = 1 << 2,
	PWT         = 1 << 3,
	PCD         = 1 << 4,
	ACCESSED    = 1 << 5,
	DIRTY       = 1 << 6,
	HUGE        = 1 << 7,
};

typedef uint32_t PageTableEntry;
typedef uint32_t PageDirectoryEntry;

typedef struct page_table_t{
    PageTableEntry entries[PAGE_TABLE_ENTRIES];
} __attribute__((aligned(PAGE_TABLE_ALIGNMENT))) PageTable;

typedef struct page_directory_t{
    PageTableEntry entries[PAGE_DIRECTORY_ENTRIES];
}__attribute__((aligned(PAGE_DIRECTORY_ALIGNMENT))) PageDirectory;

}

#endif // !PAGING_HPP
