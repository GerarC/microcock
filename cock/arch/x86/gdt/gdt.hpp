#ifndef ARCH_X86_GDT_HPP
#define ARCH_X86_GDT_HPP

/**
 * @file
 * @brief Global Descriptor Table, Task State Segments and I/O permission
 * bitmap management for i386.
 *
 * The kernel uses a flat model (base 0, limit 4 GiB) with Ring 0 and Ring 3
 * code and data segments. One TSS provides the Ring 0 stack (ESP0/SS0) used on
 * every privilege transition into the kernel and the per-thread I/O
 * permission bitmap. Two more TSS entries back task gates for the double fault
 * and NMI vectors so they run on their own stacks.
 */

#include <hal/process_context.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::arch::x86 {

/** @brief Size of the I/O Permission Bitmap inside the TSS. */
constexpr size_t IO_PERMISSION_MAP_SIZE = core::hal::IO_PERMISSION_MAP_BYTES;

/**
 * @brief Hardware-defined structure for a single GDT entry.
 */
typedef struct __attribute__((packed)) _gdt_entry_t {
	uint16_t limit;		/**< Limit bits 0..15. */
	uint16_t base_low;	/**< Base bits 0..15. */
	uint8_t base_middle; /**< Base bits 16..23. */
	uint8_t access;		/**< Present, DPL, type (code, data or TSS) bits. */
	uint8_t flags;		/**< High nibble: granularity and size; low nibble: limit
						   bits 16..19. */
	uint8_t base_high;	/**< Base bits 24..31. */
} Entry;

/**
 * @brief Pointer structure used to load the GDT via the `lgdt` instruction.
 */
typedef struct __attribute__((packed)) _gdt_pointer_t {
	uint16_t limit; /**< Size of the table in bytes minus one. */
	uintptr_t base; /**< Linear address of the first entry. */
} Pointer;

/**
 * @brief Hardware-defined Task State Segment (TSS) structure.
 * * Primarily used by the CPU during hardware interrupts to switch to a valid
 * Kernel Ring 0 stack.
 *
 * The kernel does not use hardware task switching for scheduling; the general
 * purpose fields only matter for the double fault and NMI task gates, where the
 * CPU loads them on entry.
 */
typedef struct __attribute__((packed)) _task_state_segement_base {
	uint32_t prev_tss; /**< Previous task link (low 16 bits used). */
	uint32_t esp0; /**< Ring 0 Stack Pointer (Updated on context switch). */
	uint32_t ss0;  /**< Ring 0 Stack Segment. */
	uint32_t esp1; /**< Ring 1 stack pointer, unused. */
	uint32_t ss1;  /**< Ring 1 stack segment, unused. */
	uint32_t esp2; /**< Ring 2 stack pointer, unused. */
	uint32_t ss2;  /**< Ring 2 stack segment, unused. */
	uint32_t cr3;  /**< Page directory loaded when the task is entered. */
	uint32_t eip;  /**< Entry point loaded when the task is entered. */
	uint32_t eflags; /**< EFLAGS loaded when the task is entered. */
	uint32_t eax;  /**< Saved EAX. */
	uint32_t ecx;  /**< Saved ECX. */
	uint32_t edx;  /**< Saved EDX. */
	uint32_t ebx;  /**< Saved EBX. */
	uint32_t esp;  /**< Stack pointer loaded when the task is entered. */
	uint32_t ebp;  /**< Saved EBP. */
	uint32_t esi;  /**< Saved ESI. */
	uint32_t edi;  /**< Saved EDI. */
	uint32_t es;   /**< ES selector. */
	uint32_t cs;   /**< CS selector. */
	uint32_t ss;   /**< SS selector. */
	uint32_t ds;   /**< DS selector. */
	uint32_t fs;   /**< FS selector. */
	uint32_t gs;   /**< GS selector. */
	uint32_t ldt;  /**< LDT selector, unused. */
	uint16_t trap; /**< Bit 0: raise a debug exception on task switch. */
	uint16_t iomap_base; /**< Offset to the I/O Permission Map. */
} TssBase;

/**
 * @brief The TSS of the running kernel: the hardware part followed by the I/O
 * Permission Map.
 */
typedef struct __attribute__((packed)) _task_state_segement_entry : TssBase {
	uint8_t io_permission_map
		[IO_PERMISSION_MAP_SIZE]; /**< Bitmap controlling access to `in` and
									 `out` instructions per thread. */
} TssEntry;

/**
 * @brief Manager class for the Global Descriptor Table and TSS.
 *
 * All state is static and the class is meant for a single CPU. Methods run in
 * Ring 0; setKernelStack() and applyIoPermissions() are called with interrupts
 * disabled from the context switch path.
 */
class GDT {
  public:
	/**
	 * @brief Initializes and loads the system GDT with flat Kernel and User
	 * segments.
	 *
	 * Builds the null, kernel and user code and data descriptors, the main TSS
	 * (SS0 set to the kernel data selector, all I/O ports denied) and the two
	 * fault TSS descriptors, then executes `lgdt`, reloads the segment
	 * registers (`gdt_flush`) and loads the task register with `ltr`
	 * (`tss_flush`). Must run once during early boot, before the IDT is
	 * loaded.
	 */
	static void init();

	/**
	 * @brief Updates the Ring 0 stack pointer in the TSS. Called during context
	 * switches.
	 *
	 * The CPU reads ESP0 on the next Ring 3 to Ring 0 transition.
	 *
	 * @param stack_pointer The top of the kernel stack for the incoming thread.
	 */
	static void setKernelStack(uint32_t stack_pointer);

	/**
	 * @brief Installs the I/O permission bitmap of the incoming process.
	 *
	 * Does nothing when `version` equals the version already applied. With
	 * `bytes` equal to zero the map is disabled by pointing IOMAP base past the
	 * TSS limit, so every `in` and `out` from Ring 3 faults. Otherwise the
	 * first `bytes` bytes are copied into the TSS, bytes left over from a
	 * previous larger map are set back to denied (bit set) and IOMAP base is
	 * restored. Called from the context switch with interrupts disabled.
	 *
	 * @param map Bitmap in TSS format (bit clear grants the port); at least
	 * `bytes` readable bytes, may be null only when `bytes` is zero.
	 * @param bytes Number of meaningful bytes in `map`, at most
	 * IO_PERMISSION_MAP_SIZE.
	 * @param version Version stamp of the bitmap, used to skip redundant
	 * copies.
	 */
	static void applyIoPermissions(const uint8_t *map, uint32_t bytes,
								   uint32_t version);

	/**
	 * @brief Reports which bitmap version is currently in the TSS.
	 * @return Version passed to the last effective applyIoPermissions().
	 */
	static uint32_t appliedIoVersion();

	/**
	 * @brief Gives the double fault and NMI tasks the page directory they run
	 * with. Called once the kernel page directory is final.
	 *
	 * Stores the value in the CR3 field of both fault TSS structures; the CPU
	 * loads it when it switches to those tasks.
	 *
	 * @param page_directory Physical address of the kernel page directory.
	 */
	static void bindFaultTasks(uint32_t page_directory);

  private:
	/** @brief Number of descriptors in the GDT. */
	static constexpr int MAX_ENTRIES = 8;
	/** @brief Size in bytes of the private stack of each fault task. */
	static constexpr size_t FAULT_STACK_SIZE = 0x2000;
	/** @brief The descriptor table itself. */
	static Entry entries[MAX_ENTRIES];
	/** @brief TSS of the running kernel (Ring 0 stack and I/O bitmap). */
	static TssEntry tssEntry;
	/** @brief Task entered by the double fault task gate. */
	static TssBase doubleFaultTss;
	/** @brief Task entered by the NMI task gate. */
	static TssBase nmiTss;
	/** @brief Private stack of the double fault task. */
	alignas(16) static uint8_t doubleFaultStack[FAULT_STACK_SIZE];
	/** @brief Private stack of the NMI task. */
	alignas(16) static uint8_t nmiStack[FAULT_STACK_SIZE];
	/** @brief Operand of `lgdt`. */
	static Pointer pointer;
	/** @brief Version of the bitmap currently copied in the TSS. */
	static uint32_t appliedVersion;
	/** @brief Number of bitmap bytes that may hold grants from earlier maps. */
	static uint32_t dirtyBytes;

	/**
	 * @brief Fills one descriptor.
	 * @param num Slot index in #entries.
	 * @param base Linear base address of the segment.
	 * @param limit Segment limit (20 bits used).
	 * @param access Access byte (present, DPL, type).
	 * @param gran Granularity and size flags in the high nibble.
	 */
	static void setGate(uint32_t num, uint32_t base, uint32_t limit,
						uint8_t access, uint8_t gran);

	/**
	 * @brief Builds the main TSS descriptor and clears the TSS.
	 * @param num Slot index of the TSS descriptor.
	 * @param ss0 Ring 0 stack segment selector.
	 * @param esp0 Initial Ring 0 stack pointer.
	 */
	static void writeTSS(uint32_t num, uint16_t ss0, uint32_t esp0);

	/**
	 * @brief Builds the descriptor and body of a fault task.
	 *
	 * The task runs in Ring 0 with the kernel selectors, its own stack and
	 * no I/O bitmap. CR3 is filled later by bindFaultTasks().
	 *
	 * @param num Slot index of the TSS descriptor.
	 * @param tss Task state structure to initialize.
	 * @param entry Address of the function the task starts at.
	 * @param stack Lowest address of the private stack; the top is
	 * `stack + FAULT_STACK_SIZE`.
	 */
	static void writeFaultTSS(uint32_t num, TssBase &tss, uint32_t entry,
							  uint8_t *stack);

	/** @brief Body of the double fault task: panics and never returns. */
	[[noreturn]] static void onDoubleFault();
	/** @brief Body of the NMI task: panics and never returns. */
	[[noreturn]] static void onNmi();
};

} // namespace cock::arch::x86

#endif // !GDT_HPP
