# Kernel Main Initialization

The kernel initialization process is split into two phases: architecture-specific early initialization and architecture-agnostic core initialization. Interrupts stay **disabled** through the whole of it; they are enabled for the first time by the `iret` that starts the first thread.

## Phase 1: Early Initialization (`init_cock`)
The bootloader passes control to `init_cock` in `cock/arch/x86/init.cpp`.

```cpp
extern "C" void init_cock(uint32_t magic, uint32_t boot_info_phys) {
	core::hal::console_init();
	MBInfo *boot_info = boot_memory_init(magic, boot_info_phys);
	core_init(boot_info, boot_info_phys);
	call_global_constructors();
	interrupts_init();

	MultibootModuleLoader::init(boot_info);
	cock_main(MultibootModuleLoader::getModules(), MultibootModuleLoader::getCount());
}
```

In order:

1. **`console_init`**: starts the serial port (COM1) and clears the screen. From here on the log goes out through both.
2. **`boot_memory_init`**: checks the Multiboot magic, prepares the early window and maps the information structure.
3. **`core_init`**: loads the GDT and TSS, starts the PMM from the memory map, and starts paging (kernel tables reserved, global pages on, kernel code and constants made read only and `CR0.WP` turned on). It then sets `CR0.EM`, so that floating point, MMX and SSE instructions fault: programs must not use the FPU until its state is saved per process.
4. **Global constructors**, once the heap works.
5. **`interrupts_init`**: remaps the PIC with every line masked, loads the IDT, starts the timer and the system call table.
6. **Modules**: reads the Multiboot module list.

## Phase 2: Core Initialization (`cock_main`)
The architecture-independent kernel logic begins at `cock_main` in `cock/core/cock.cpp`. It sets up the MLFQ scheduler, creates the idle thread, and loads every Multiboot module as a process of its own.

```cpp
for (size_t i = 0; i < mod_count; i++) {
	PhysAddr new_cr3 = VirtualMemoryManager::createAddressSpace();
	...
	LoadedImage image;
	if (!load_executable(modules[i].start_address, modules[i].size, new_cr3, image)) {
		VirtualMemoryManager::destroyAddressSpace(new_cr3);
		continue;
	}

	Process *process = Process::create(new_cr3, image.user_space_limit);
	process->setName(modules[i].name);
	if (strcmp(modules[i].name, ROOT_SERVER_MODULE) == 0)
		grant_root_capabilities(*process);
	Thread *app_thread = new Thread(*process, image.entry, ThreadPriority::NORMAL);
	...
	Scheduler::addThread(app_thread);
}

Thread *reaper_thread = new Thread(kernel_reaper, ThreadPriority::HIGH);
TaskManager::setReaper(reaper_thread);
Scheduler::addThread(reaper_thread);

core::hal::manual_timer();
```

For each module:

1. A new address space is created that shares the kernel half.
2. The ELF loader validates the image (class, machine, segment bounds, entry point), maps the segments writable, copies them, and then seals the read-only ones. A module that fails is skipped, and its address space is destroyed.
3. A `Process` is created with the user-space limit of the ELF class, and named after the module.
4. The module called `init` is given the root capabilities (see [capabilities](capabilities.md)); no other module is.
5. A thread is created for it, with its user stack.
6. If the module is called `vga_driver`, the kernel console detaches from the screen and remembers the driver as the destination of `sys_write` output.

After the modules, the **reaper** thread is created (see [task management](task_management.md#garbage-collection)); it comes last so the process ids of the modules are the same on every boot. Finally `manual_timer()` raises the timer vector by software. The scheduler finds no current thread, picks the first ready one, and the `iret` that enters it turns interrupts on. `cock_main` never continues past that point.

## Test mode
When built with `COCK_TEST_MODE`, `cock_main` runs the test suite right after the banner, before any scheduling, and exits QEMU through `isa-debug-exit` with a status the test script turns into its own exit code. A suite that needs threads calls `TaskManager::init()` itself and retires them through the graveyard.
