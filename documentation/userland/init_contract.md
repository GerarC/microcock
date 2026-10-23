# Replacing `init`

The kernel has no program called `init`. It has a first program: the first Multiboot module that is an ELF image. Anything that fills that place and does what `init` does can replace it. This page says what the kernel gives that program and what it must do.

## What the kernel gives the first program

- **The root capabilities**, with the right to grant them: every I/O port, every interrupt line, all physical memory, all boot modules, `SPAWN` and the `IPC` capability over every service label.
- **The service label `ROOT_SERVICE`**, so it may talk to every other program.
- **The boot module list** it can ask about with `SYS_MODULE_INDEX`, and the right to open a module with `SYS_MODULE_OPEN`.
- **Nothing else**: no argument, no environment, no file system. Every other program starts with an empty capability set.

The kernel loads every ELF module of the boot list before the scheduler runs (`vga_driver` and `ramfs` are modules too, because nothing could load them), and gives the root capabilities to the first one only. Which module comes first is the order of the `module` lines of `grub.cfg`, written by the Makefile from `USERLAND_BOOT_ORDER`.

## What the first program must do

The `init` of the tree does these things; a replacement does them in its own way, or does without the ones its system does not use.

1. **Give `ramfs` the `initrd`**: `SYS_MODULE_INDEX("initrd")`, `SYS_CAP_GRANT` of a `BOOT_MODULE` capability for that index, then a `CAPABILITY_GRANTED` message to `ramfs`. Without it there is no file system, and every program that needs a file waits for ever.
2. **Label the boot modules** with `SYS_SERVICE_SET` and give each the `IPC` capabilities of its policy, because they started before anyone could set the rights at birth.
3. **Start the other programs** with `cock::user::spawn` (header `cock/user/spawn.hpp`), reading each image from the `initrd` through `ramfs`. `spawn` takes the label and the IPC labels the new program is born with, so they exist before its first instruction.
4. **Grant what each program needs**: ports, lines, device memory (`SYS_CAP_GRANT`), then `CAPABILITY_GRANTED` to the program, which waits for it.
5. **Watch the programs that must come back**: start them with the exit notice requested and start them again on `PROCESS_EXITED`.

The kernel checks every grant against what the first program holds, so a replacement cannot give more than it has, and a bug in it cannot give a program more than the root capabilities.

## How to put another program first

1. Write the program in `userland/<name>/` with the layout of any other program (`Makefile`, `README.md`, `src/`, `include/<name>/`) and `extern "C" int main()`.
2. Put its name first in `USERLAND_BOOT_ORDER` in the root `Makefile`, before `ramfs` and `vga_driver`. A replacement that keeps `init` can leave the list as it is and put the new name before `init`.
3. Build with `make iso`. A different first program is a different ISO: there is no boot parameter that chooses it yet.

Any 32 bit ELF executable of the machine of the kernel (fixed addresses, no relocations, no dynamic linking) can be a boot module, so the program does not have to be built in this tree.

## What it cannot do

- It cannot be an ELF of another architecture, a position independent executable or a program that needs a dynamic linker.
- It cannot use the Linux system call interface: the system calls are the ones of [ipc.md](../core/ipc.md) and the README. Running software written for Linux needs a compatibility layer in Ring 3 that does not exist.

See also [init](../../userland/init/README.md), [capabilities](../core/capabilities.md) and [task management](../core/task_management.md#spawn).
