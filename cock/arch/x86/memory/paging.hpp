#ifndef PAGING_HPP
#define PAGING_HPP
#include <cock/core/memory/address.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

constexpr uint32_t PAGE_TABLE_ENTRIES = 0x400;
constexpr uint32_t PAGE_DIRECTORY_ENTRIES = 0x400;
constexpr uint32_t PAGE_TABLE_ALIGNMENT = 0x1000;
constexpr uint32_t PAGE_DIRECTORY_ALIGNMENT = 0x1000;

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
	GLOBAL = 1 << 8,
	NOT_OWNED = 1 << 9,
};

typedef uint32_t PageTableEntry;
typedef uint32_t PageDirectoryEntry;

typedef struct page_table_t {
	PageTableEntry entries[PAGE_TABLE_ENTRIES];
} __attribute__((aligned(PAGE_TABLE_ALIGNMENT))) PageTable;

typedef struct page_directory_t {
	PageTableEntry entries[PAGE_DIRECTORY_ENTRIES];
} __attribute__((aligned(PAGE_DIRECTORY_ALIGNMENT))) PageDirectory;

class Paging {
  public:
	static void init();

	static bool mapPage(core::memory::PhysAddr phys, core::memory::VirtAddr virt,
						uint32_t flags);
	static void unmapPage(core::memory::VirtAddr virt);
	static uint32_t getPageFlags(core::memory::VirtAddr virt);
	static bool updatePageFlags(core::memory::VirtAddr virt, uint32_t flags);
	static core::memory::PhysAddr translate(core::memory::VirtAddr virt);
	static bool isRangeAccessible(core::memory::VirtAddr virt, size_t length,
								  bool user, bool write);
	static void invalidate(core::memory::VirtAddr virt);

	static core::memory::PhysAddr createAddressSpace();
	static void destroyAddressSpace(core::memory::PhysAddr directory);
	static core::memory::PhysAddr kernelDirectory();

	static bool globalPagesEnabled();
	static void protectKernelImage();

  private:
	static void reserveKernelTables();
	static void enableGlobalPages();
};

} // namespace cock::arch::x86

extern "C" uint32_t page_directory[cock::arch::x86::PAGE_DIRECTORY_ENTRIES];

#endif // !PAGING_HPP
