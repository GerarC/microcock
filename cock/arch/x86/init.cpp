#include "cock/arch/x86/boot/multiboot.hpp"
#include "cock/arch/x86/memory/memory.hpp"
#include "cock/utils/logger.hpp"
#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/arch/x86/interrupt/idt/idt.hpp>
#include <cock/arch/x86/keyboard/keyboard.hpp>
#include <cock/arch/x86/timer/timer.hpp>
#include <cock/core/cock.hpp>
#include <cock/driver/vga.hpp>
#include <stdint.h>

#if defined(__linux__)
#error "You are not using a  cross-compiler"
#endif

namespace cock::arch::x86 {

using cock::driver::vga_instance;
using cock::utils::Logger;

GDT gdt;
IDT idt;
Timer timer;
Keyboard keyboard;
MemoryManager mm;

extern "C" void call_global_constructors();
void video_init();
void core_init();

extern "C" void init_cock(uint32_t magic, MBInfo *boot_info) {

	video_init();
	call_global_constructors();
	core_init();
	// Logger::trace("init_cock: magic=0x%x, boot_info=0x%x", magic, boot_info);
	boot_info = reinterpret_cast<MBInfo *>(
		reinterpret_cast<uintptr_t>(boot_info) + 0xC0000000);
	Logger::info("boot_info = 0x%x", boot_info);
	Logger::info("magic = 0x%x", magic);
	Logger::info("first_mmap_addr = 0x%x", boot_info->mmap_address);

	if (!boot_info) {
		Logger::error("boot_info NULL!");
		FOR_ETERNAL;
	}
	mm.init(boot_info);

	cock_main();
}

void video_init() {
	static driver::VGA vga;
	vga_instance = &vga;
	vga_instance->init();
	vga_instance->clear();
}

void core_init() {
	gdt.init();
	idt.init();
	timer.init();
	keyboard.init();
}

} // namespace cock::arch::x86
