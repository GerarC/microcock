# Learning path

A reading order for someone who wants to understand MicroCOCK from the first instruction to the programs that run on top of it. It assumes C or C++ and a rough idea of what a kernel is. It does not assume you know x86.

Follow the stages in order. Each one names the pages to read, the files to open, what to look for, and a small exercise that proves you understood it. Do not read a whole folder: open only the files listed.

Paths with `cock/arch/x86` are written as `arch/` and `cock/core` as `core/`.

## Before you start

* Build and boot it once so everything you read has a visible effect: `make iso HOST_TOOLCHAIN=1`, then `make run-headless HOST_TOOLCHAIN=1` (the toolchain runs under WSL or Linux with `nasm`, `qemu-system-x86`, `grub-pc-bin` and `xorriso`).
* Keep the serial log open while you read. The kernel logs most steps of its initialization at the debug level (GDT, physical memory, paging, interrupts, timer, boot modules), so the log is a table of contents of stages 2 to 5. The console, the boot memory window and the global constructors print nothing.
* Read [project structure](project_structure.md) first. It explains the one rule that organizes everything: the core never includes `cock/arch/...`, it includes `<hal/...>`.

## Stage 0: the shape of the system

**Read:** [project structure](project_structure.md), then the indexes [core](core/README.md), [arch](arch/README.md) and [userland](userland/README.md).

**Understand:**
* There are three layers: the architecture (`cock/arch/x86`), the core (`cock/core`) and Ring 3 programs (`userland/`).
* It is a microkernel. The kernel keeps only what a program cannot do: privileged instructions, page tables, the scheduler and the capability checks. Drivers and file systems are programs.
* The HAL (`arch/include/hal/`) is the contract between the first two layers.

**Exercise:** list every header in `arch/include/hal/` and say, for each one, why the core cannot simply include something from `cock/arch` directly.

## Stage 1: from power on to C++

**Read:** [bootloader](arch/bootloader.md).

**Open:**
1. `arch/boot/entry.s`: the Multiboot header, the boot page tables and the jump to the higher half.
2. `arch/linker.ld`: where each section lands and the assert that keeps the kernel inside the first 4 MiB.
3. `arch/boot/multiboot.hpp` and `arch/boot/boot_memory.cpp`: how the structures GRUB leaves in memory are reached.
4. `arch/init.cpp`: `init_cock`, the order of the early initialization.

**Look for:** why the kernel runs at a high virtual address, and what has to be true before the first C++ function can run (stack, paging, constructors).

**Exercise:** change nothing, but find in the serial log the lines printed during `init_cock`, match each to the call that printed it, and name the steps that print nothing.

## Stage 2: the CPU tables and interrupts

**Read:** [GDT](arch/gdt.md) and [IDT](arch/idt.md).

**Open:**
1. `arch/gdt/gdt.cpp` and `segments.hpp`: segments, the TSS and the I/O permission bitmap.
2. `arch/interrupt/idt/`, `arch/interrupt/isr/isr.s` and `isr.cpp`: how an exception or a system call becomes a C++ call.
3. `arch/interrupt/irq/`, `arch/interrupt/pic/` and `arch/timer/`: hardware interrupts and the timer.
4. `core/events.cpp`: what the **core** does with whatever the architecture reports. This is the best place to see the split between mechanism and policy.

**Look for:** the architecture never calls the scheduler. It reports an event and `core::events` decides.

**Exercise:** trace a page fault from the CPU to `core::events` and say who decides whether the process dies.

## Stage 3: memory

**Read:** [memory management](core/memory_management.md).

**Open, in this order:**
1. `core/memory/address.hpp`: `PhysAddr` and `VirtAddr` as distinct types.
2. `arch/memory/pmm.cpp`: the frame bitmap.
3. `arch/memory/paging.cpp` and `layout.hpp`: page tables and address spaces.
4. `core/memory/vmm.hpp` and `arch/memory/vmm.cpp`: the policy over the mechanism, and `isUserRangeValid`.
5. `core/memory/heap.cpp`: the kernel heap.
6. `core/memory/shared_memory.cpp`: shared objects, holders and release.

**Look for:** every pointer that comes from a program is checked with `isUserRangeValid` before it is used. Search for it with Grep and read three call sites.

**Exercise:** follow `SYS_SHM_CREATE` from `core/syscall/mm/sys_shm.cpp` to the frames it takes, and say who pays for them.

## Stage 4: processes, threads and scheduling

**Read:** [task management](core/task_management.md).

**Open:**
1. `core/task/process.hpp` and `thread.hpp`: what a process owns and what a thread owns.
2. `core/task/task_manager.cpp`: the pid registry, the graveyard and the reaper.
3. `core/task/scheduler.cpp`: the multilevel feedback queue.
4. `arch/task/tasking.cpp` and `context.cpp`: the initial stack of a new thread and the per-process I/O context.
5. `core/task/timeouts.cpp` and `futex.cpp`: sleeping and waiting.
6. `core/cock.cpp`: `cock_main`, which loads the boot modules and starts the idle and reaper threads.

**Look for:** a system call never sleeps inside its handler. It marks the thread, asks for a reschedule and a restart, and returns; the call runs again when the thread wakes.

**Exercise:** read `sys_ipc_recv` and explain how a thread blocks on an empty inbox without blocking the kernel.

## Stage 5: system calls and messages

**Read:** [IPC](core/ipc.md).

**Open:**
1. `arch/syscall/syscall_nums.hpp` and `syscall_manager.cpp`: the numbers and the table.
2. `core/syscall/ipc/sys_ipc.cpp`: send and receive, and `sender_may_use`, the gate on messages that carry authority.
3. `core/task/ipc.cpp`: `deliver` and `notify`.
4. `core/task/sync_ipc.cpp` and `core/syscall/ipc/sys_call.cpp`: synchronous calls with direct handoff and deferred replies.
5. `shared/include/cock/ipc/message.hpp` and `shared/include/cock/abi/call.hpp`: the layouts both sides agree on.

**Look for:** how a hardware interrupt reaches a driver as a message (`core/irq/irq_routing.cpp`), which is what makes drivers programs.

**Exercise:** write down the life of one synchronous call: who blocks, who runs, who replies and what happens if the server dies in the middle.

## Stage 6: authority

**Read:** [capabilities](core/capabilities.md).

**Open:**
1. `core/security/capability.hpp`: `CapabilitySet`, `covers` and `mayGrant`.
2. `core/syscall/security/sys_cap.cpp` and `hw/sys_request_resource.cpp`: where the checks happen.
3. `shared/include/cock/abi/capability.hpp`: the capability types.

**Look for:** a denied call returns `-1` and logs at trace level, never above, so a program cannot flood the log.

**Exercise:** say which capability a keyboard driver needs and trace the call that checks it.

## Stage 7: loading and starting programs

**Read:** the boot modules and spawn parts of [userland](userland/userland.md) and [filesystem](userland/filesystem.md).

**Open:**
1. `core/loader/elf.cpp`, `elf_loader.hpp` and `elf32.hpp`: the loader.
2. `userland/lib/include/cock/user/spawn.hpp`, `shared/include/cock/elf/image.hpp` and `core/syscall/proc/sys_proc_*.cpp`: how a program is created from an image, the loader in user space and the primitives of the kernel.
3. `core/syscall/mm/sys_module_open.cpp`: data boot modules such as the initrd.
4. `initrd/manifest.conf`: who gets which capabilities.

**Exercise:** read the manifest, pick a program and list what the kernel has to do, in order, for it to appear.

## Stage 8: your first program

**Read:** [userland](userland/userland.md), then the README of the [user library](../userland/lib/README.md) and of [rs](../userland/servers/rs/README.md).

**Open:**
1. `userland/linker.ld` and `userland/common.mk`: how every program is linked.
2. `userland/lib/include/cock/user/api.hpp`: the C++ wrappers of the system calls.
3. `libc/include/sys/syscalls.h`: the raw system call and why it must not address memory through `EBP`.
4. `userland/servers/rs/src/main.cpp`: the root server and its policy.

**Look for:** every program defines `main` and `userland/lib/src/crt0.cpp` is the entry that calls it. Most programs end with `exit_process`; servers such as `sh` and `vga_driver` loop forever on their input instead.

**Exercise:** read a small program (`userland/drivers/rtc/src/requests.cpp` is a good one), then write down which message it receives and what it answers.

## Stage 9: the drivers and servers

Take them in this order, because each builds on the one before. For each, read the page and the program README, then `main.cpp`.

1. **Display and keyboard:** [VGA](userland/vga.md), [keyboard](userland/keyboard.md). The smallest drivers; the keyboard shows an interrupt becoming a message.
2. **Console:** [console](userland/console.md). A server with clients, an input line and deferred replies.
3. **Block storage:** [block storage](userland/block_storage.md). A port I/O driver with partition parsing.
4. **Files:** [filesystem](userland/filesystem.md), then [FAT32](userland/fat32.md) and [ext2](userland/ext2.md). The `vfs` joins the servers behind one tree.
5. **The shell:** [shell](userland/shell.md). It uses the console and the `vfs` through the C library, so it exercises the whole path from a keystroke to a disk.

**Exercise:** follow one command in the shell, such as `cat /ext/small.txt`, through every process it touches, and write down each message.

## Stage 10: the tests, which are also the best examples

**Read:** the README of [test/userland](../test/userland/README.md) and the [fuzzer](userland/fuzzer.md).

**Open:**
1. `test/initializer.cpp` and one suite under `test/cock/`: tests that run inside the kernel.
2. `test/host/`: tests of `shared/` that run on the host, with no QEMU.
3. `test/userland/probe/`: security and ABI checks seen from Ring 3.
4. `scripts/qemu_userland_test.sh`: the end to end run, and what it requires of the serial log.

**Exercise:** pick a rule from the Kernel safety section of the project rules, find the probe check that proves it, and break the rule on purpose to see the check fail.

## After the path

* The TODO list in the [README](../README.md) says what is open.
* Public headers carry the reference of each function. When you meet a function you do not know, read its header before its body.
* To change something, the project rules apply: validate every pointer and number a program gives you, name every constant, keep the core free of `cock/arch`, and update the matching page here in the same change.
