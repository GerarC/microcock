# Kernel Main Initialization

The kernel initialization process is split into two phases: architecture-specific early initialization and architecture-agnostic core initialization.

## Phase 1: Early Initialization (`init_cock`)
The bootloader passes control to `init_cock` in `cock/arch/x86/init.cpp`. It performs early video initialization, sets up the GDT/IDT via `core_init`, and parses Multiboot modules.

```cpp
extern "C" void init_cock(uint32_t magic, MBInfo *boot_info) {
    video_init();
    core_init(boot_info);
    call_global_constructors();
    Logger::trace("magic = 0x%x", magic);

    MultibootModuleLoader::init(boot_info);
    cock_main(MultibootModuleLoader::getModules(), MultibootModuleLoader::getCount());
}
```

## Phase 2: Core Initialization (`cock_main`)
The architecture-independent kernel logic begins at `cock_main` in `cock/core/cock.cpp`. It sets up the MLFQ scheduler, creates the idle thread, and loads userland ELF modules into their own address spaces.

```cpp
extern "C" void cock_main(const BootModule* modules, size_t mod_count) {
    TaskManager::init();
    Scheduler::init();

    core::hal::block_interruptions();
    Thread *idle_thread = new Thread(kernel_idle, ThreadPriority::IDLE);
    Scheduler::addThread(idle_thread);

    for (size_t i = 0; i < mod_count; i++) {
        uintptr_t new_cr3 = VirtualMemoryManager::createAddressSpace();
        uintptr_t entry = load_executable(modules[i].start_address, new_cr3);
        
        Thread *app_thread = new Thread(entry, new_cr3, ThreadPriority::NORMAL);
        
        // Handoff to Userland VGA Server
        if (strcmp(modules[i].name, "vga_driver") == 0) {
            driver::vga_instance->setEnabled(false);
            vga_server_pid = app_thread->getId(); 
            Logger::info("VGA driver detected. Assigned PID: %d", vga_server_pid);
        }

        Scheduler::addThread(app_thread);
    }
    core::hal::unblock_interruptions();
    core::hal::manual_timer();
    
    FOR_ETERNAL;
}
```
