#include <cock/arch/x86/boot/boot_memory.hpp>
#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/arch/x86/interrupt/idt/idt.hpp>
#include <cock/arch/x86/interrupt/pic/pic.hpp>
#include <cock/arch/x86/loader/multiboot_module_loader.hpp>
#include <cock/arch/x86/memory/paging.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/arch/x86/timer/timer.hpp>
#include <cock/core/cock.hpp>
#include <cock/core/syscall/syscall_manager.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <hal/console.hpp>
#include <stdint.h>

#if defined(__linux__)
#error "You are not using a  cross-compiler"
#endif

namespace cock::arch::x86 {

using cock::core::syscall::SyscallManager;
using cock::utils::Logger;
using cock::utils::panic;
using core::memory::PhysAddr;

constexpr uint8_t PIC_MASTER_OFFSET = 0x20;
constexpr uint8_t PIC_SLAVE_OFFSET = 0x28;

extern "C" void call_global_constructors();
MBInfo *boot_memory_init(uint32_t magic, uint32_t boot_info_phys);
void core_init(MBInfo *boot_info, uint32_t boot_info_phys);
void interrupts_init();

extern "C" void init_cock(uint32_t magic, uint32_t boot_info_phys) {

	core::hal::console_init();
	MBInfo *boot_info = boot_memory_init(magic, boot_info_phys);
	core_init(boot_info, boot_info_phys);
	call_global_constructors();
	interrupts_init();

	MultibootModuleLoader::init(boot_info);
	cock_main(MultibootModuleLoader::getModules(), MultibootModuleLoader::getCount());
}

MBInfo *boot_memory_init(uint32_t magic, uint32_t boot_info_phys) {
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
		panic("Invalid multiboot magic 0x%x", magic);
	if (boot_info_phys == 0) panic("boot_info NULL!");

	BootMemory::init();
	MBInfo *boot_info = static_cast<MBInfo *>(
		BootMemory::map(PhysAddr(boot_info_phys), sizeof(MBInfo)));
	if (!boot_info) panic("boot_info is unreachable");

	return boot_info;
}

void core_init(MBInfo *boot_info, uint32_t boot_info_phys) {
	GDT::init();
	PhysicalMemoryManager::init(boot_info, PhysAddr(boot_info_phys));
	Paging::init();
}

void interrupts_init() {
	PIC_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);
	IDT::init();
	Timer::init();
	SyscallManager::init();
}

} // namespace cock::arch::x86
