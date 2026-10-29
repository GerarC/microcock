# Replacing `rs`

The kernel has no program called `rs`. It has a first program: the first Multiboot module that is an ELF image. Anything that fills that place and does what `rs` does can replace it. This page says what the kernel gives that program and what it must do.

## What the kernel gives the first program

- **The root capabilities**, with the right to grant them: every I/O port, every interrupt line, all physical memory, all boot modules, `SPAWN`, the `IPC` capability over every service label and the `ENDPOINT` capability over every endpoint, with the grant, mint and bind rights.
- **The service label `ROOT_SERVICE`**, under which the name server and the policy know it. It reaches a program through a handle on the endpoint of that program, which it makes itself with the `ENDPOINT` capability it holds.
- **The boot module list** it can ask about with `SYS_MODULE_INDEX`, and the right to open a module with `SYS_MODULE_OPEN`.
- **Nothing else**: no argument, no environment, no file system. Every other program starts with an empty capability set.

The kernel loads every ELF module of the boot list before the scheduler runs (`names`, `vga_driver` and `ramfs` are modules too, because nothing could load them), and gives the root capabilities to the first one only. Which module comes first is the order of the `module` lines of `grub.cfg`, written by the Makefile from `USERLAND_BOOT_ORDER`.

## What the first program must do

The `rs` of the tree does these things; a replacement does them in its own way, or does without the ones its system does not use.

1. **Bring up the name server**: find the boot module `names` with `SYS_MODULE_PROCESS`, label it `NAMES_SERVICE`, give it an `ENDPOINT` capability over every endpoint (with `GRANT_DELEGATE` and `GRANT_MINT`, so it can choose the badge of each handle), make its endpoint and bind it to it, make the handle the first program calls it through, and send it `CAPABILITY_GRANTED`. Every program the first program starts later gets a handle on that endpoint as its initial handle (`SpawnRights::initialEndpoint`), so the name server needs no fixed process id. Then describe the system to it: a `DEFINE` per program, an `ALLOW` per `ipc=` name and a `BIND` per started process (see [names](../servers/names.md)). Before each `BIND`, `rs` makes the endpoint of the label the first time (`SYS_EP_CREATE`) and binds it to the new process (`SYS_EP_BIND`), so a restarted server receives on the endpoint its clients already hold handles on. Without it no program can connect to another.
2. **Give `ramfs` the `initrd`**: `SYS_MODULE_INDEX("initrd")`, `SYS_CAP_GRANT` of a `BOOT_MODULE` capability for that index, then a `CAPABILITY_GRANTED` message to `ramfs`. Without it there is no file system, and every program that needs a file waits for ever.
3. **Label the boot modules** with `SYS_SERVICE_SET` and bind each to the name server, because they started before anyone could set the label at birth.
4. **Start the other programs**: the `rs` of the tree asks the process manager `pm` (see [pm](../servers/pm.md)); a first program that has none starts them itself with `cock::user::spawn` (header `cock/user/spawn.hpp`), reading each image from the `initrd` through `ramfs`. `spawn` takes the label and the initial handle the new program is born with, so they exist before its first instruction; the handles on other programs come from the name server.
5. **Grant what each program needs**: ports, lines, device memory (`SYS_CAP_GRANT`), then `CAPABILITY_GRANTED` to the program, which waits for it. The message goes through a handle on the endpoint of the program, which has to be bound first.
6. **Watch the programs that must come back**: start them with the exit notice requested and start them again on `PROCESS_EXITED` (or on the `PM_EXITED` that `pm` forwards), binding the new process to the name server.

The kernel checks every grant against what the first program holds, so a replacement cannot give more than it has, and a bug in it cannot give a program more than the root capabilities.

## How to put another program first

1. Write the program in `userland/<name>/` with the layout of any other program (`Makefile`, `README.md`, `src/`, `include/<name>/`) and `extern "C" int main()`.
2. Put its name first in `USERLAND_BOOT_ORDER` in the root `Makefile`, before `names`, `ramfs` and `vga_driver`. A replacement that keeps `rs` can leave the list as it is and put the new name before `rs`.
3. Build with `make iso`. A different first program is a different ISO: there is no boot parameter that chooses it yet.

Any 32 bit ELF executable of the machine of the kernel (fixed addresses, no relocations, no dynamic linking) can be a boot module, so the program does not have to be built in this tree.

## What it cannot do

- It cannot be an ELF of another architecture, a position independent executable or a program that needs a dynamic linker.
- It cannot use the Linux system call interface: the system calls are the ones of [ipc.md](../../core/ipc.md) and the README. Running software written for Linux needs a compatibility layer in Ring 3 that does not exist.

See also [rs](../../../userland/servers/rs/README.md), [capabilities](../../core/capabilities.md) and [spawn](../../core/process/spawn.md).
