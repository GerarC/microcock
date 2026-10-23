#include <cock/core/boot/module.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/cock.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/hal/utils.hpp>
#include <cock/core/security/capability.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/version.hpp>
#include <cock/utils/logger.hpp>
#include <cock/core/loader/elf.hpp>
#include <cock/core/memory/shared_memory.hpp>
#include <hal/boot.hpp>
#include <hal/console.hpp>
#include <hal/interrupt.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef COCK_TEST_MODE
#include <test/initializer.hpp>
#endif

namespace cock {

using core::security::capability_tree;
using core::security::Capability;
using core::security::CapabilityType;
using core::security::NO_CAPABILITY_NODE;
using core::security::CAPABILITY_FLAG_GRANT;
using core::task::Process;
using core::task::Scheduler;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadResult;
using core::task::ThreadState;
using core::boot::BootModule;
using core::memory::PhysAddr;
using core::memory::VirtualMemoryManager;
using core::loader::has_elf_magic;
using core::loader::load_executable;
using core::memory::SharedMemory;
using core::loader::LoadedImage;
using utils::Logger;
using utils::LogLevel;

uint32_t debug_output_pid = 0;

static void grant_root_capability(Process &process, CapabilityType type, uint64_t count) {
	Capability capability;
	capability.type = type;
	capability.flags = CAPABILITY_FLAG_GRANT;
	capability.count = count;
	capability.node =
		capability_tree.createRoot(process.getId(), type, capability.first, count);
	if (capability.node == NO_CAPABILITY_NODE)
		Logger::error("No derivation node for a root capability of type %u",
					  static_cast<unsigned>(type));
	process.getCapabilities().add(capability);
}

static void grant_root_capabilities(Process &process, size_t module_count) {
	grant_root_capability(process, CapabilityType::IO_PORTS, core::hal::IO_PORT_COUNT);
	grant_root_capability(process, CapabilityType::IRQ, core::hal::IRQ_LINE_COUNT);
	grant_root_capability(process, CapabilityType::PHYSICAL_MEMORY,
						  core::hal::PHYSICAL_ADDRESS_SPACE_SIZE);
	grant_root_capability(process, CapabilityType::BOOT_MODULE, module_count);
}

static void register_data_module(const BootModule &module, size_t index) {
	size_t pages = (module.size + core::hal::PAGE_MASK) >> core::hal::PAGE_SHIFT;
	uint32_t object =
		SharedMemory::createFixed(module.name, index, module.physical_address, pages);
	if (object == core::memory::NO_SHARED_OBJECT) {
		Logger::error("Cannot keep data module %s", module.name);
		return;
	}
	Logger::info("Data module %s: %u bytes, object %u", module.name,
				 static_cast<unsigned>(module.size), static_cast<unsigned>(object));
}

ThreadResult kernel_idle() {
	Logger::info("Idle thread running");
	while (true)
		core::hal::halt();
	return ThreadResult::success();
}

ThreadResult kernel_reaper() {
	while (true) {
		TaskManager::buryDeadThreads();
		{
			core::hal::InterruptGuard guard;
			if (TaskManager::graveyardEmpty())
				Scheduler::getCurrentThread()->setState(ThreadState::BLOCKED);
		}
		Scheduler::yield();
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

    bool root_task_started = false;
    for (size_t i = 0; i < mod_count; i++) {
        if (!has_elf_magic(modules[i].start_address, modules[i].size)) {
            register_data_module(modules[i], i);
            continue;
        }

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
        Thread *app_thread = new Thread(*process, image.entry, ThreadPriority::NORMAL);
        if (!root_task_started) {
            grant_root_capabilities(*process, mod_count);
            root_task_started = true;
        }

        Scheduler::addThread(app_thread);
    }

	size_t released_frames = 0;
	for (size_t i = 0; i < mod_count; i++) {
		if (!has_elf_magic(modules[i].start_address, modules[i].size)) continue;
		VirtualMemoryManager::releaseBootFrames(modules[i].physical_address, modules[i].size);
		released_frames += modules[i].size >> core::hal::PAGE_SHIFT;
	}
	core::hal::boot_memory_release();
	Logger::debug("Boot memory released (about %u frames of program images)",
				  static_cast<unsigned>(released_frames));

	Thread *reaper_thread = new Thread(kernel_reaper, ThreadPriority::HIGH);
	TaskManager::setReaper(reaper_thread);
	Scheduler::addThread(reaper_thread);

	core::hal::manual_timer();
	Logger::fatal("Kernel panic: Returned to cock_main!");
	FOR_ETERNAL;
}

} // namespace cock
