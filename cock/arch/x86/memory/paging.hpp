#ifndef PAGING_HPP
#define PAGING_HPP

/**
 * @file
 * @brief Two level i386 paging: entry flags, table types and the Paging
 * manager.
 *
 * The active page directory is edited through the recursive mapping (see
 * layout.hpp), so every operation below acts on the address space that is
 * currently loaded in CR3. All of it runs in Ring 0.
 */

#include <cock/core/memory/address.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

/** @brief Entries in one page table. */
constexpr uint32_t PAGE_TABLE_ENTRIES = 0x400;
/** @brief Entries in one page directory. */
constexpr uint32_t PAGE_DIRECTORY_ENTRIES = 0x400;
/** @brief Required alignment in bytes of a page table. */
constexpr uint32_t PAGE_TABLE_ALIGNMENT = 0x1000;
/** @brief Required alignment in bytes of a page directory. */
constexpr uint32_t PAGE_DIRECTORY_ALIGNMENT = 0x1000;

/**
 * @brief Bits of a page table or page directory entry (low 12 bits).
 *
 * The upper 20 bits of an entry hold the physical frame number.
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
	GLOBAL = 1 << 8, /**< Translation survives CR3 reloads (needs CR4.PGE);
						used for kernel pages. */
	NOT_OWNED = 1 << 9, /**< Software bit (ignored by the CPU): the frame
						   belongs to someone else (shared memory, device), so
						   destroyAddressSpace() must not free it. */
};

/** @brief A page table entry: frame address plus PageFlag bits. */
typedef uint32_t PageTableEntry;
/** @brief A page directory entry: table frame address plus PageFlag bits. */
typedef uint32_t PageDirectoryEntry;

/** @brief One page table: 1024 entries covering 4 MiB, 4 KiB aligned. */
typedef struct page_table_t {
	PageTableEntry entries[PAGE_TABLE_ENTRIES];
} __attribute__((aligned(PAGE_TABLE_ALIGNMENT))) PageTable;

/** @brief One page directory: 1024 entries covering 4 GiB, 4 KiB aligned. */
typedef struct page_directory_t {
	PageTableEntry entries[PAGE_DIRECTORY_ENTRIES];
} __attribute__((aligned(PAGE_DIRECTORY_ALIGNMENT))) PageDirectory;

/**
 * @brief Page table manager of the currently loaded address space.
 *
 * Functions that allocate frames use the PhysicalMemoryManager. They do not
 * take locks by themselves except where noted; the kernel runs them with
 * interrupts masked or on a single path.
 */
class Paging {
  public:
	/**
	 * @brief Finishes the kernel paging setup after the boot directory is live.
	 *
	 * Preallocates a page table for every kernel half directory slot (so all
	 * address spaces can share them), turns on global pages when the CPU
	 * supports them and write protects the kernel text and read-only data.
	 * Requires the physical memory manager. Once, during boot.
	 */
	static void init();

	/**
	 * @brief Maps one frame at a virtual address.
	 *
	 * Allocates the page table when missing (user accessible when `flags` has
	 * USER) and invalidates the TLB entry. Kernel half mappings get GLOBAL when
	 * global pages are active and USER is not set.
	 *
	 * @param phys Physical frame to map; the offset bits are ignored.
	 * @param virt Virtual page to map.
	 * @param flags PageFlag bits (low 12 bits are kept).
	 * @return False if USER is requested at or above the kernel base, a page
	 * table could not be allocated, or the page is already mapped; true on
	 * success.
	 */
	static bool mapPage(core::memory::PhysAddr phys, core::memory::VirtAddr virt,
						uint32_t flags);

	/**
	 * @brief Clears the mapping of one page and invalidates its TLB entry.
	 *
	 * Does not free the frame. Silently returns if no page table covers
	 * `virt`.
	 *
	 * @param virt Virtual page to unmap.
	 */
	static void unmapPage(core::memory::VirtAddr virt);

	/**
	 * @brief Reads the flag bits of a mapping.
	 * @param virt Virtual address to inspect.
	 * @return The low 12 bits of the entry, or zero when the page is not
	 * present.
	 */
	static uint32_t getPageFlags(core::memory::VirtAddr virt);

	/**
	 * @brief Replaces the flag bits of an existing mapping, keeping the frame.
	 *
	 * Invalidates the TLB entry. Setting USER also marks the directory entry
	 * user accessible.
	 *
	 * @param virt Mapped virtual page.
	 * @param flags New PageFlag bits.
	 * @return False if the page is not mapped or USER is requested in the kernel
	 * half; true on success.
	 */
	static bool updatePageFlags(core::memory::VirtAddr virt, uint32_t flags);

	/**
	 * @brief Resolves a virtual address to a physical address.
	 * @param virt Virtual address in the active address space.
	 * @return Physical address including the page offset, or a null PhysAddr
	 * when not mapped.
	 */
	static core::memory::PhysAddr translate(core::memory::VirtAddr virt);

	/**
	 * @brief Checks that a whole range is mapped with the required rights.
	 *
	 * This is the building block for validating user pointers: every page of
	 * the range must be present, and (when asked) user accessible and writable,
	 * at both the directory and the table level. A zero length range is
	 * accepted.
	 *
	 * @param virt First byte of the range.
	 * @param length Number of bytes.
	 * @param user True to require Ring 3 access (also rejects ranges reaching
	 * the kernel half).
	 * @param write True to require write permission.
	 * @return True if every page satisfies the checks and the range does not
	 * wrap around 4 GiB.
	 */
	static bool isRangeAccessible(core::memory::VirtAddr virt, size_t length,
								  bool user, bool write);

	/**
	 * @brief Drops one page from the TLB (`invlpg`).
	 * @param virt Virtual page to invalidate.
	 */
	static void invalidate(core::memory::VirtAddr virt);

	/**
	 * @brief Creates a new address space that shares the kernel half.
	 *
	 * Allocates a directory frame, copies the kernel half entries from the
	 * current directory, clears the user half and installs the recursive slot.
	 * Runs with interrupts disabled and uses a temporary kernel mapping.
	 *
	 * @return Physical address of the new directory, or a null PhysAddr if
	 * memory ran out.
	 */
	static core::memory::PhysAddr createAddressSpace();

	/**
	 * @brief Releases an address space.
	 *
	 * Frees every user page frame (except those marked NOT_OWNED), every user
	 * page table and the directory. Ignores a null address, the kernel
	 * directory and the directory loaded in CR3. Runs with interrupts disabled.
	 *
	 * @param directory Physical address returned by createAddressSpace().
	 */
	static void destroyAddressSpace(core::memory::PhysAddr directory);

	/**
	 * @brief Gives the physical address of the boot (kernel) page directory.
	 * @return Physical address of #page_directory.
	 */
	static core::memory::PhysAddr kernelDirectory();

	/**
	 * @brief Tells whether kernel mappings are marked global.
	 * @return True when init() enabled CR4.PGE.
	 */
	static bool globalPagesEnabled();

	/**
	 * @brief Makes kernel code and read-only data pages non writable.
	 *
	 * Clears the writable bit of every page between the linker symbols for the
	 * text and rodata sections, invalidates them and sets CR0.WP so Ring 0
	 * obeys it. Called from init().
	 */
	static void protectKernelImage();

  private:
	/** @brief Allocates and clears a page table for each missing kernel half
	 * directory slot, so new address spaces can copy them. */
	static void reserveKernelTables();
	/** @brief Marks present kernel pages global and sets CR4.PGE, if the CPU
	 * supports it. */
	static void enableGlobalPages();
};

} // namespace cock::arch::x86

/** @brief The boot page directory (symbol defined in the entry assembly). */
extern "C" uint32_t page_directory[cock::arch::x86::PAGE_DIRECTORY_ENTRIES];

#endif // !PAGING_HPP
