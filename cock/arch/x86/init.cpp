#include <cock/arch/x86/loader/multiboot_module_loader.hpp>
#include <cock/arch/x86/boot/multiboot.hpp>
#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/arch/x86/interrupt/idt/idt.hpp>
#include <cock/arch/x86/keyboard/keyboard.hpp>
#include <cock/arch/x86/memory/pmm.hpp>
#include <cock/arch/x86/timer/timer.hpp>
#include <cock/core/cock.hpp>
#include <cock/core/syscall/syscall_manager.hpp>
#include <cock/driver/vga.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

#if defined(__linux__)
#error "You are not using a  cross-compiler"
#endif

namespace cock::arch::x86 {

using cock::core::syscall::SyscallManager;
using cock::driver::vga_instance;
using cock::utils::Logger;

extern "C" void call_global_constructors();
void video_init();
void core_init(MBInfo *boot_info);

extern "C" void init_cock(uint32_t magic, MBInfo *boot_info) {

	video_init();
	core_init(boot_info);
	call_global_constructors();
	Logger::trace("magic = 0x%x", magic);

    MultibootModuleLoader::init(boot_info);
    cock_main(MultibootModuleLoader::getModules(), MultibootModuleLoader::getCount());
}

void video_init() {
	static driver::VGA vga;
	vga_instance = &vga;
	vga_instance->init();
	vga_instance->clear();
}

void core_init(MBInfo *boot_info) {
	GDT::init();
	IDT::init();
	Timer::init();
	Keyboard::init();
	SyscallManager::init();

	if (!boot_info) {
		Logger::error("boot_info NULL!");
		FOR_ETERNAL;
	}

	PhysicalMemoryManager::init(boot_info);
}

} // namespace cock::arch::x86
