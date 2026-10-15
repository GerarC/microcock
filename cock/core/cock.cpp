#include <cock/core/boot/module.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/cock.hpp>
#include <cock/core/hal/utils.hpp>
#include <cock/core/security/capability.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/version.hpp>
#include <cock/utils/logger.hpp>
#include <cock/core/loader/elf.hpp>
#include <hal/console.hpp>
#include <hal/interrupt.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef COCK_TEST_MODE
#include <test/initializer.hpp>
#endif

namespace cock {

using core::security::Capability;
using core::security::CapabilityType;
using core::security::CAPABILITY_FLAG_GRANT;
using core::task::Process;
using core::task::Scheduler;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadResult;
using core::boot::BootModule;
using core::memory::PhysAddr;
using core::memory::VirtualMemoryManager;
using core::loader::load_executable;
using core::loader::LoadedImage;
using utils::Logger;
using utils::LogLevel;

uint32_t vga_server_pid = 0;

constexpr const char *VGA_DRIVER_MODULE = "vga_driver";
constexpr const char *ROOT_SERVER_MODULE = "init";

static void grant_root_capabilities(Process &process) {
	Capability ports;
	ports.type = CapabilityType::IO_PORTS;
	ports.flags = CAPABILITY_FLAG_GRANT;
	ports.count = core::hal::IO_PORT_COUNT;

	Capability irqs;
	irqs.type = CapabilityType::IRQ;
	irqs.flags = CAPABILITY_FLAG_GRANT;
	irqs.count = core::hal::IRQ_LINE_COUNT;

	Capability memory;
	memory.type = CapabilityType::PHYSICAL_MEMORY;
	memory.flags = CAPABILITY_FLAG_GRANT;
	memory.count = core::hal::PHYSICAL_ADDRESS_SPACE_SIZE;

	process.getCapabilities().add(ports);
	process.getCapabilities().add(irqs);
	process.getCapabilities().add(memory);
}

ThreadResult kernel_idle() {
	while (true) {
		TaskManager::buryDeadThreads();
		core::hal::halt();
	}
	return ThreadResult::success();
}

extern "C" void cock_main(const BootModule* modules, size_t mod_count) {
	Logger::init(LogLevel::DEBUG);
	printf("Welcome to %s\n", core::VERSION_STRING);
	printf("Build: %s (%s)\n", COCK_BUILD_DATE, COCK_GIT_HASH);
	puts("Semillero de Linux UdeA");
	puts("SEIC UdeA");

#ifdef COCK_TEST_MODE
	Logger::warn("Setting Test mode.");
	test::run_all_tests();
	Logger::info("All test where executed.");
	FOR_ETERNAL;
#endif

	TaskManager::init();
	Scheduler::init();

	Thread *idle_thread = new Thread(kernel_idle, ThreadPriority::IDLE);
	Scheduler::setIdleThread(idle_thread);

    for (size_t i = 0; i < mod_count; i++) {
        Logger::info("Loading module: %s", modules[i].name);

        PhysAddr new_cr3 = VirtualMemoryManager::createAddressSpace();
        if (new_cr3.isNull()) {
            Logger::error("Cannot create an address space for module %s", modules[i].name);
            continue;
        }

        LoadedImage image;
        if (!load_executable(modules[i].start_address, modules[i].size, new_cr3, image)) {
            Logger::error("Cannot load module %s", modules[i].name);
            VirtualMemoryManager::destroyAddressSpace(new_cr3);
            continue;
        }

        Process *process = Process::create(new_cr3, image.user_space_limit);
        process->setName(modules[i].name);
        if (strcmp(modules[i].name, ROOT_SERVER_MODULE) == 0)
            grant_root_capabilities(*process);
        Thread *app_thread = new Thread(*process, image.entry, ThreadPriority::NORMAL);
        if (strcmp(modules[i].name, VGA_DRIVER_MODULE) == 0) {
            core::hal::console_detach();
            vga_server_pid = app_thread->getId();
            Logger::info("VGA driver detected. Assigned PID: %d", vga_server_pid);
        }

        Scheduler::addThread(app_thread);
    }

	core::hal::manual_timer();
	Logger::fatal("Kernel panic: Returned to cock_main!");
	FOR_ETERNAL;
}

} // namespace cock
