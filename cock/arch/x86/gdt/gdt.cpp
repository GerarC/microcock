#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/arch/x86/gdt/segments.hpp>
#include <cock/utils/logger.hpp>
#include <stddef.h>
#include <string.h>

extern "C" void gdt_flush(uintptr_t); // gdt flush
extern "C" void tss_flush();		  // tss flush

namespace cock::arch::x86 {

using cock::utils::Logger;

constexpr size_t IO_MAP_BASE = 0x68;
constexpr uint8_t TSS_ACCESS = 0x89;
constexpr uint8_t TSS_GRANULARITY = 0x00;
constexpr uint8_t IO_DENIED = 0xFF;

Entry GDT::entries[MAX_ENTRIES];
TssEntry GDT::tssEntry;
Pointer GDT::pointer;

void GDT::init() {
	pointer.limit = (sizeof(Entry) * MAX_ENTRIES - 1);
	pointer.base = reinterpret_cast<uintptr_t>(&entries);

	GDT::setGate(0, 0, 0, 0, 0);				// NULL segment
	GDT::setGate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code Segment
	GDT::setGate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data Segment
	GDT::setGate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code Segment
	GDT::setGate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data Segment
	GDT::writeTSS(5, KERNEL_DATA_SELECTOR, 0x0);

	gdt_flush(reinterpret_cast<uintptr_t>(&pointer));
	tss_flush();

	Logger::debug("GTD Initialized");
}

void GDT::setGate(uint32_t num, uint32_t base, uint32_t limit, uint8_t access,
				  uint8_t gran) {
	entries[num].base_low = base & 0xFFFF;
	entries[num].base_middle = (base >> 0x10) & 0xFF;
	entries[num].base_high = (base >> 0x18) & 0xFF;

	entries[num].limit = (limit & 0xFFFF);
	entries[num].flags = (limit >> 0x10) & 0x0F;
	entries[num].flags |= gran & 0xF0;

	entries[num].access = access;
}

void GDT::writeTSS(uint32_t num, uint16_t ss0, uint32_t esp0) {
	size_t tss_size = sizeof(tssEntry);
	uint32_t base = reinterpret_cast<uint32_t>(&tssEntry);
	uint32_t limit = tss_size - 1;

	GDT::setGate(num, base, limit, TSS_ACCESS, TSS_GRANULARITY);
	memset(&tssEntry, 0, tss_size);

	tssEntry.ss0 = ss0;
	tssEntry.esp0 = esp0;

	tssEntry.iomap_base = IO_MAP_BASE;
	memset(tssEntry.io_permission_map, IO_DENIED,
		   sizeof(tssEntry.io_permission_map));
}

void GDT::setIOPM(const uint8_t *thread_iopm) {
	if (thread_iopm) memcpy(tssEntry.io_permission_map, thread_iopm, sizeof(tssEntry.io_permission_map));
	else memset(tssEntry.io_permission_map, IO_DENIED, sizeof(tssEntry.io_permission_map));
}
void GDT::setKernelStack(uint32_t stack_pointer) {
	tssEntry.esp0 = stack_pointer;
}

} // namespace cock::arch::x86
