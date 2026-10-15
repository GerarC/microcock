#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/arch/x86/gdt/segments.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stddef.h>
#include <string.h>

extern "C" void gdt_flush(uintptr_t); // gdt flush
extern "C" void tss_flush();		  // tss flush

namespace cock::arch::x86 {

using cock::utils::Logger;
using cock::utils::panic;

constexpr uint32_t TSS_SLOT = 5;
constexpr uint32_t DOUBLE_FAULT_TSS_SLOT = 6;
constexpr uint32_t NMI_TSS_SLOT = 7;
constexpr uint16_t FAULT_TSS_IO_MAP_BASE = sizeof(TssBase);
constexpr size_t IO_MAP_BASE = 0x68;
constexpr uint8_t TSS_ACCESS = 0x89;
constexpr uint8_t TSS_GRANULARITY = 0x00;
constexpr uint8_t IO_DENIED = 0xFF;
constexpr uint16_t IO_MAP_DISABLED = sizeof(TssEntry);
constexpr uint32_t NOTHING_APPLIED = 0;

Entry GDT::entries[MAX_ENTRIES];
TssEntry GDT::tssEntry;
TssBase GDT::doubleFaultTss;
TssBase GDT::nmiTss;
uint8_t GDT::doubleFaultStack[FAULT_STACK_SIZE];
uint8_t GDT::nmiStack[FAULT_STACK_SIZE];
Pointer GDT::pointer;
uint32_t GDT::appliedVersion = NOTHING_APPLIED;
uint32_t GDT::dirtyBytes = 0;

void GDT::init() {
	pointer.limit = (sizeof(Entry) * MAX_ENTRIES - 1);
	pointer.base = reinterpret_cast<uintptr_t>(&entries);

	GDT::setGate(0, 0, 0, 0, 0);				// NULL segment
	GDT::setGate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code Segment
	GDT::setGate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data Segment
	GDT::setGate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code Segment
	GDT::setGate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data Segment
	GDT::writeTSS(TSS_SLOT, KERNEL_DATA_SELECTOR, 0x0);
	GDT::writeFaultTSS(DOUBLE_FAULT_TSS_SLOT, doubleFaultTss,
					   reinterpret_cast<uint32_t>(&onDoubleFault), doubleFaultStack);
	GDT::writeFaultTSS(NMI_TSS_SLOT, nmiTss, reinterpret_cast<uint32_t>(&onNmi),
					   nmiStack);

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

void GDT::writeFaultTSS(uint32_t num, TssBase &tss, uint32_t entry,
						uint8_t *stack) {
	GDT::setGate(num, reinterpret_cast<uint32_t>(&tss), sizeof(TssBase) - 1,
				 TSS_ACCESS, TSS_GRANULARITY);
	memset(&tss, 0, sizeof(TssBase));

	tss.eip = entry;
	tss.esp = reinterpret_cast<uint32_t>(stack) + FAULT_STACK_SIZE;
	tss.eflags = EFLAGS_RESERVED_BIT;
	tss.cs = KERNEL_CODE_SELECTOR;
	tss.ss = KERNEL_DATA_SELECTOR;
	tss.ds = KERNEL_DATA_SELECTOR;
	tss.es = KERNEL_DATA_SELECTOR;
	tss.fs = KERNEL_DATA_SELECTOR;
	tss.gs = KERNEL_DATA_SELECTOR;
	tss.iomap_base = FAULT_TSS_IO_MAP_BASE;
}

void GDT::bindFaultTasks(uint32_t page_directory) {
	doubleFaultTss.cr3 = page_directory;
	nmiTss.cr3 = page_directory;
}

void GDT::onDoubleFault() {
	panic("double fault (kernel stack overflow or a fault while handling a "
		  "fault) EIP 0x%x ESP 0x%x",
		  tssEntry.eip, tssEntry.esp);
}

void GDT::onNmi() {
	panic("non maskable interrupt EIP 0x%x ESP 0x%x", tssEntry.eip,
		  tssEntry.esp);
}

void GDT::applyIoPermissions(const uint8_t *map, uint32_t bytes,
							 uint32_t version) {
	if (appliedVersion == version) return;
	appliedVersion = version;

	if (bytes == 0) {
		tssEntry.iomap_base = IO_MAP_DISABLED;
		return;
	}

	memcpy(tssEntry.io_permission_map, map, bytes);
	if (dirtyBytes > bytes)
		memset(tssEntry.io_permission_map + bytes, IO_DENIED, dirtyBytes - bytes);
	dirtyBytes = bytes;
	tssEntry.iomap_base = IO_MAP_BASE;
}

uint32_t GDT::appliedIoVersion() { return appliedVersion; }

void GDT::setKernelStack(uint32_t stack_pointer) {
	tssEntry.esp0 = stack_pointer;
}

} // namespace cock::arch::x86
