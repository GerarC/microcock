#ifndef ARCH_X86_GDT_HPP
#define ARCH_X86_GDT_HPP
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

/** @brief Size of the I/O Permission Bitmap inside the TSS. */
constexpr size_t IO_PERMISSION_MAP_SIZE = 8193;

/**
 * @brief Hardware-defined structure for a single GDT entry.
 */
typedef struct __attribute__((packed)) _gdt_entry_t {
	uint16_t limit;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t flags;
	uint8_t base_high;
} Entry;

/**
 * @brief Pointer structure used to load the GDT via the `lgdt` instruction.
 */
typedef struct __attribute__((packed)) _gdt_pointer_t {
	uint16_t limit;
	uintptr_t base;
} Pointer;

/**
 * @brief Hardware-defined Task State Segment (TSS) structure.
 * * Primarily used by the CPU during hardware interrupts to switch to a valid
 * Kernel Ring 0 stack.
 */
typedef struct __attribute__((packed)) _task_state_segement_entry {
	uint32_t prev_tss;
	uint32_t esp0; /**< Ring 0 Stack Pointer (Updated on context switch). */
	uint32_t ss0;  /**< Ring 0 Stack Segment. */
	uint32_t esp1;
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;
	uint32_t cs;
	uint32_t ss;
	uint32_t ds;
	uint32_t fs;
	uint32_t gs;
	uint32_t ldt;
	uint16_t trap;
	uint16_t iomap_base; /**< Offset to the I/O Permission Map. */
	uint8_t io_permission_map
		[IO_PERMISSION_MAP_SIZE]; /**< Bitmap controlling access to `in` and
									 `out` instructions per thread. */
} TssEntry;

/**
 * @brief Manager class for the Global Descriptor Table and TSS.
 */
class GDT {
  public:
	GDT();

	/**
	 * @brief Initializes and loads the system GDT with flat Kernel and User
	 * segments.
	 */
	static void init();

	/**
	 * @brief Updates the Ring 0 stack pointer in the TSS. Called during context
	 * switches.
	 * @param stack_pointer The top of the kernel stack for the incoming thread.
	 */
	static void setKernelStack(uint32_t stack_pointer);

	/**
	 * @brief Applies an I/O permission bitmap to the TSS to grant/deny hardware
	 * port access.
	 * @param thread_iopm Pointer to the thread's I/O permission map, or nullptr
	 * to deny all.
	 */
	static void setIOPM(const uint8_t *thread_iopm);

  private:
	static constexpr int MAX_ENTRIES = 6;
	static Entry entries[MAX_ENTRIES];
	static TssEntry tssEntry;
	static Pointer pointer;

	static void setGate(uint32_t num, uint32_t base, uint32_t limit,
						uint8_t access, uint8_t gran);
	static void writeTSS(uint32_t num, uint16_t ss0, uint32_t esp0);
};

} // namespace cock::arch::x86

#endif // !GDT_HPP
