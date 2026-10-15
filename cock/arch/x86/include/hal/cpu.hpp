#ifndef ARCH_X86_HAL_CPU_HPP
#define ARCH_X86_HAL_CPU_HPP

#include <cock/arch/x86/syscall/syscall_nums.hpp>
#include <stdint.h>

namespace cock::core::hal {

constexpr uint32_t TIMER_IRQ_LINE = 0;
constexpr uint64_t IO_PORT_COUNT = 0x10000;
constexpr uint64_t PHYSICAL_ADDRESS_SPACE_SIZE = 0x100000000ull;
constexpr uint8_t TIMER_VECTOR = 0x20;
constexpr uint8_t SYSCALL_VECTOR = 0x80;
constexpr uint16_t IO_DELAY_PORT = 0x80;
constexpr uint8_t IO_DELAY_VALUE = 0;
constexpr uint16_t EMULATOR_EXIT_PORT = 0xF4;
constexpr uint8_t SYSCALL_INSTRUCTION_SIZE = 2;
constexpr uint32_t CPUID_FEATURES_LEAF = 1;
constexpr uint32_t CPUID_EDX_GLOBAL_PAGES = 1u << 13;
constexpr uint32_t CR4_GLOBAL_PAGES = 1u << 7;
constexpr uint32_t CR0_EMULATE_FPU = 1u << 2;
constexpr uint32_t CR0_WRITE_PROTECT = 1u << 16;

[[gnu::always_inline]] inline void halt() { __asm__ volatile("hlt"); }

[[gnu::always_inline]] inline uintptr_t save_and_block_interruptions() {
	uintptr_t flags;
	__asm__ volatile("pushf; pop %0; cli" : "=r"(flags) : : "memory");
	return flags;
}

[[gnu::always_inline]] inline void restore_interruptions(uintptr_t flags) {
	__asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
}

[[gnu::always_inline]] inline void manual_timer() {
	__asm__ volatile("int %0" : : "i"(static_cast<int>(TIMER_VECTOR)));
}

[[gnu::always_inline]] inline void yield() {
	__asm__ volatile("int %0"
					 :
					 : "i"(static_cast<int>(SYSCALL_VECTOR)),
					   "a"(arch::x86::SyscallNum::SYS_SCHED_YIELD)
					 : "memory");
}

[[gnu::always_inline]] inline void port_out8(uint16_t port, uint8_t value) {
	__asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

[[gnu::always_inline]] inline uint8_t port_in8(uint16_t port) {
	uint8_t value;
	__asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

[[gnu::always_inline]] inline void io_delay() {
	port_out8(IO_DELAY_PORT, IO_DELAY_VALUE);
}

[[gnu::always_inline]] inline void invalidate_page(uintptr_t address) {
	__asm__ volatile("invlpg (%0)" : : "r"(address) : "memory");
}

[[gnu::always_inline]] inline uint32_t read_page_table_root() {
	uint32_t value;
	__asm__ volatile("mov %%cr3, %0" : "=r"(value));
	return value;
}

[[gnu::always_inline]] inline void write_page_table_root(uint32_t value) {
	__asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

[[gnu::always_inline]] inline uint32_t read_cr0() {
	uint32_t value;
	__asm__ volatile("mov %%cr0, %0" : "=r"(value));
	return value;
}

[[gnu::always_inline]] inline void write_cr0(uint32_t value) {
	__asm__ volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}

[[gnu::always_inline]] inline void enable_write_protect() {
	write_cr0(read_cr0() | CR0_WRITE_PROTECT);
}

[[gnu::always_inline]] inline void disable_fpu() {
	write_cr0(read_cr0() | CR0_EMULATE_FPU);
}

[[gnu::always_inline]] inline uint32_t read_cr4() {
	uint32_t value;
	__asm__ volatile("mov %%cr4, %0" : "=r"(value));
	return value;
}

[[gnu::always_inline]] inline void write_cr4(uint32_t value) {
	__asm__ volatile("mov %0, %%cr4" : : "r"(value) : "memory");
}

[[gnu::always_inline]] inline bool cpu_supports_global_pages() {
	uint32_t eax = CPUID_FEATURES_LEAF;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	__asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
	return (edx & CPUID_EDX_GLOBAL_PAGES) != 0;
}

[[gnu::always_inline]] inline void enable_global_pages() {
	write_cr4(read_cr4() | CR4_GLOBAL_PAGES);
}

[[gnu::always_inline]] inline uint32_t read_fault_address() {
	uint32_t value;
	__asm__ volatile("mov %%cr2, %0" : "=r"(value));
	return value;
}

[[gnu::always_inline]] inline void exit_emulator(uint8_t code) {
	port_out8(EMULATOR_EXIT_PORT, code);
}

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_CPU_HPP
