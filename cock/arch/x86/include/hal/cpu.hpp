#ifndef ARCH_X86_HAL_CPU_HPP
#define ARCH_X86_HAL_CPU_HPP

/**
 * @file
 * @brief HAL: CPU primitives, I/O ports, control registers and platform
 * constants.
 *
 * Every function is a single privileged instruction (or a short sequence)
 * forced inline, so the core pays no call cost. All of them must run in Ring 0
 * unless stated otherwise.
 *
 * Port requirement: an architecture must provide the same names with the same
 * meaning: halt(), save_and_block_interruptions(), restore_interruptions(),
 * manual_timer(), yield(), invalidate_page(), read_page_table_root(),
 * write_page_table_root(), read_fault_address(), exit_emulator() and the
 * constants TIMER_IRQ_LINE, TIMER_FREQUENCY_HZ, IO_PORT_COUNT,
 * PHYSICAL_ADDRESS_SPACE_SIZE, TIMER_VECTOR, SYSCALL_VECTOR and
 * SYSCALL_INSTRUCTION_SIZE. Port I/O (port_in8, port_out8, io_delay) may be
 * omitted by architectures without a separate I/O space, as long as the core
 * code that uses it stays behind the port. The control register and CPUID
 * helpers are x86 only.
 */

#include <cock/arch/x86/syscall/syscall_nums.hpp>
#include <stdint.h>

namespace cock::core::hal {

/** @brief PIC input line that carries the system timer (PIT channel 0). */
constexpr uint32_t TIMER_IRQ_LINE = 0;
/** @brief Frequency in Hz at which the scheduler tick fires. */
constexpr uint32_t TIMER_FREQUENCY_HZ = 100;
/** @brief Number of I/O ports an x86 CPU can address (16 bit port space). */
constexpr uint64_t IO_PORT_COUNT = 0x10000;
/** @brief Size in bytes of the 32 bit physical address space (4 GiB). */
constexpr uint64_t PHYSICAL_ADDRESS_SPACE_SIZE = 0x100000000ull;
/** @brief Interrupt vector of the timer IRQ after the PIC remap (0x20 + 0). */
constexpr uint8_t TIMER_VECTOR = 0x20;
/** @brief Interrupt vector of the system call gate (`int 0x80`). */
constexpr uint8_t SYSCALL_VECTOR = 0x80;
/** @brief Unused port that takes about one microsecond to write; used as a
 * delay. */
constexpr uint16_t IO_DELAY_PORT = 0x80;
/** @brief Value written to IO_DELAY_PORT. */
constexpr uint8_t IO_DELAY_VALUE = 0;
/** @brief Port of the QEMU `isa-debug-exit` device. */
constexpr uint16_t EMULATOR_EXIT_PORT = 0xF4;
/** @brief Length in bytes of the `int imm8` instruction (opcode plus vector). */
constexpr uint8_t SYSCALL_INSTRUCTION_SIZE = 2;
/** @brief CPUID leaf that reports the feature flags in EDX and ECX. */
constexpr uint32_t CPUID_FEATURES_LEAF = 1;
/** @brief CPUID leaf 1 EDX bit 13: the CPU supports global pages (PGE). */
constexpr uint32_t CPUID_EDX_GLOBAL_PAGES = 1u << 13;
/** @brief CR4 bit 7 (PGE): enables global page table entries. */
constexpr uint32_t CR4_GLOBAL_PAGES = 1u << 7;
/** @brief CR0 bit 2 (EM): x87 instructions raise #NM, so the FPU is off. */
constexpr uint32_t CR0_EMULATE_FPU = 1u << 2;
/** @brief CR0 bit 16 (WP): Ring 0 writes honor read-only pages. */
constexpr uint32_t CR0_WRITE_PROTECT = 1u << 16;

/**
 * @brief Stops the CPU until the next interrupt (`hlt`).
 *
 * Interrupts must be enabled or the CPU stays halted until NMI or reset.
 */
[[gnu::always_inline]] inline void halt() { __asm__ volatile("hlt"); }

/**
 * @brief Disables maskable interrupts and returns the previous state.
 *
 * Executes `pushf; pop; cli` and acts as a compiler memory barrier. Pair it
 * with restore_interruptions(); this is what InterruptGuard uses.
 *
 * @return The EFLAGS value before `cli` (bit 9 tells whether interrupts were
 * enabled).
 */
[[gnu::always_inline]] inline uintptr_t save_and_block_interruptions() {
	uintptr_t flags;
	__asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");
	return flags;
}

/**
 * @brief Restores the interrupt state saved by save_and_block_interruptions().
 *
 * Executes `push; popf`, which reloads the whole EFLAGS image: interrupts
 * come back on only if they were on when it was saved.
 *
 * @param flags Value previously returned by save_and_block_interruptions().
 */
[[gnu::always_inline]] inline void restore_interruptions(uintptr_t flags) {
	__asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
}

/**
 * @brief Fires the timer interrupt by software (`int 0x20`).
 *
 * Forces the scheduler tick path without waiting for the PIT: the CPU enters
 * the vector 0x20 stub exactly as it would for IRQ0.
 */
[[gnu::always_inline]] inline void manual_timer() {
	__asm__ volatile("int %0" : : "i"(static_cast<int>(TIMER_VECTOR)));
}

/**
 * @brief Gives up the CPU through the system call gate.
 *
 * Executes `int 0x80` with EAX set to SYS_SCHED_YIELD. Usable from Ring 0 and
 * Ring 3. Returns when the scheduler picks the caller again.
 */
[[gnu::always_inline]] inline void yield() {
	__asm__ volatile("int %0"
					 :
					 : "i"(static_cast<int>(SYSCALL_VECTOR)),
					   "a"(arch::x86::SyscallNum::SYS_SCHED_YIELD)
					 : "memory");
}

/**
 * @brief Writes a byte to an I/O port (`outb`).
 *
 * In Ring 3 this faults unless the TSS I/O permission bitmap allows the port.
 *
 * @param port Port number (0..0xFFFF).
 * @param value Byte to write.
 */
[[gnu::always_inline]] inline void port_out8(uint16_t port, uint8_t value) {
	__asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * @brief Reads a byte from an I/O port (`inb`).
 *
 * In Ring 3 this faults unless the TSS I/O permission bitmap allows the port.
 *
 * @param port Port number (0..0xFFFF).
 * @return The byte read.
 */
[[gnu::always_inline]] inline uint8_t port_in8(uint16_t port) {
	uint8_t value;
	__asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

/**
 * @brief Waits roughly one I/O bus cycle by writing to a scratch port.
 *
 * Needed between some PIC programming steps on old hardware.
 */
[[gnu::always_inline]] inline void io_delay() {
	port_out8(IO_DELAY_PORT, IO_DELAY_VALUE);
}

/**
 * @brief Drops one page from the current TLB (`invlpg`).
 *
 * Required after changing or clearing a page table entry that may be cached.
 * Affects only the running CPU.
 *
 * @param address Any virtual address inside the page to invalidate.
 */
[[gnu::always_inline]] inline void invalidate_page(uintptr_t address) {
	__asm__ volatile("invlpg (%0)" : : "r"(address) : "memory");
}

/**
 * @brief Reads the physical address of the active page directory (CR3).
 * @return CR3 value (directory base plus the PWT and PCD flag bits).
 */
[[gnu::always_inline]] inline uint32_t read_page_table_root() {
	uint32_t value;
	__asm__ volatile("mov %%cr3, %0" : "=r"(value));
	return value;
}

/**
 * @brief Switches the address space by loading CR3.
 *
 * Flushes all non global TLB entries. The code executing this must be mapped
 * identically in the old and the new space (kernel half is shared).
 *
 * @param value Physical address of the new page directory, page aligned.
 */
[[gnu::always_inline]] inline void write_page_table_root(uint32_t value) {
	__asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

/** Shift that places the high half of the time stamp counter above the low half. */
constexpr unsigned CYCLE_COUNTER_HIGH_SHIFT = 32;

/**
 * @brief Reads the time stamp counter.
 * @return CPU cycles since reset. It is monotonic on the CPU that reads it; the
 * rate is not known to the caller, so measure it against the timer tick.
 */
[[gnu::always_inline]] inline uint64_t read_cycle_counter() {
	uint32_t low;
	uint32_t high;
	__asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
	return (static_cast<uint64_t>(high) << CYCLE_COUNTER_HIGH_SHIFT) | low;
}

/**
 * @brief Reads control register CR0.
 * @return Current CR0 value.
 */
[[gnu::always_inline]] inline uint32_t read_cr0() {
	uint32_t value;
	__asm__ volatile("mov %%cr0, %0" : "=r"(value));
	return value;
}

/**
 * @brief Writes control register CR0.
 * @param value New CR0 value; callers should read-modify-write to keep the
 * other bits (PE, PG, ...).
 */
[[gnu::always_inline]] inline void write_cr0(uint32_t value) {
	__asm__ volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}

/**
 * @brief Makes Ring 0 respect read-only pages (sets CR0.WP).
 *
 * Used after the kernel text and read-only data are remapped read only.
 */
[[gnu::always_inline]] inline void enable_write_protect() {
	write_cr0(read_cr0() | CR0_WRITE_PROTECT);
}

/**
 * @brief Turns the FPU off (sets CR0.EM) so no FPU state is ever live.
 *
 * Any x87 instruction then raises the device not available exception (#NM).
 */
[[gnu::always_inline]] inline void disable_fpu() {
	write_cr0(read_cr0() | CR0_EMULATE_FPU);
}

/**
 * @brief Reads control register CR4.
 *
 * Only valid on CPUs that implement CR4 (Pentium and later).
 *
 * @return Current CR4 value.
 */
[[gnu::always_inline]] inline uint32_t read_cr4() {
	uint32_t value;
	__asm__ volatile("mov %%cr4, %0" : "=r"(value));
	return value;
}

/**
 * @brief Writes control register CR4.
 * @param value New CR4 value; callers should read-modify-write.
 */
[[gnu::always_inline]] inline void write_cr4(uint32_t value) {
	__asm__ volatile("mov %0, %%cr4" : : "r"(value) : "memory");
}

/**
 * @brief Asks CPUID whether global pages (PGE) are available.
 *
 * Executes `cpuid` with EAX equal to CPUID_FEATURES_LEAF and tests bit 13 of
 * EDX. Serializing and slow, call it only during boot.
 *
 * @return True if CR4.PGE can be enabled.
 */
[[gnu::always_inline]] inline bool cpu_supports_global_pages() {
	uint32_t eax = CPUID_FEATURES_LEAF;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	__asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
	return (edx & CPUID_EDX_GLOBAL_PAGES) != 0;
}

/**
 * @brief Enables global pages by setting CR4.PGE.
 *
 * Precondition: cpu_supports_global_pages() returned true. Entries marked
 * global then survive CR3 reloads.
 */
[[gnu::always_inline]] inline void enable_global_pages() {
	write_cr4(read_cr4() | CR4_GLOBAL_PAGES);
}

/**
 * @brief Reads the linear address that caused the last page fault (CR2).
 *
 * Valid only inside the page fault handler before another fault can overwrite
 * it.
 *
 * @return Faulting virtual address.
 */
[[gnu::always_inline]] inline uint32_t read_fault_address() {
	uint32_t value;
	__asm__ volatile("mov %%cr2, %0" : "=r"(value));
	return value;
}

/**
 * @brief Stops the QEMU virtual machine with an exit status.
 *
 * Writes `code` to the isa-debug-exit port; QEMU exits with
 * `(code << 1) | 1`. Does nothing on real hardware, so callers must still
 * halt afterwards.
 *
 * @param code Status byte to report to the host.
 */
[[gnu::always_inline]] inline void exit_emulator(uint8_t code) {
	port_out8(EMULATOR_EXIT_PORT, code);
}

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_CPU_HPP
