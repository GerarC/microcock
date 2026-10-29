# vga_driver

The display server. It maps the VGA text buffer into its own address space and draws the characters it receives by IPC. It is a reference driver: the screen is not in the kernel, so the console server, the keyboard driver and any other program write to the display through messages.

## Where it sits

- It is a Multiboot module (`vga_driver` is in `USERLAND_BOOT_ORDER`) and not a file of the initrd, so it already runs when `init` starts. The kernel does not know it by name: when it maps the screen the kernel logs `Screen mapped by PID` and detaches its own console; `SYS_WRITE` text of the kernel and of programs without the console client is forwarded to it.
- Capabilities: physical memory `mem:0xA0000+0x20000` (the video window) and the CRT controller ports `io:0x3D4+2` (the hardware cursor), granted by `init` through the manifest line `vga_driver caps=mem:0xA0000+0x20000,io:0x3D4+2`, which has no `path=`. After every output call and every `show` the driver writes the cursor position of the terminal on the screen to the CRT controller (registers `0x0E` and `0x0F`), so the blinking cursor follows the text. The test manifest adds nothing for it.
- Its clients are `console` (all text, the echo and the clear request) and the kernel (the `SYS_WRITE` path, in chunks of 20 bytes).
- It opts out of the console client (`CONSOLE_CLIENT = no`): the server draws the console output.

## Protocol

Message layout: `shared/include/cock/ipc/message.hpp`. The server only receives; it never replies.

| Message | Content | Effect |
|---------|---------|--------|
| `CAPABILITY_GRANTED` | none used | First one maps the buffer (`MAP_PHYS`, `MAP_FIXED`) and marks the server ready |
| `OUTPUT_WRITE_CHAR` | `arg0` is the character | Draws it; `\n` goes to the next line, Backspace moves one column back and blanks the cell (nothing at column 0) |
| `OUTPUT_WRITE_STR` | `arg0` length, the text starts at `arg1` | Draws up to 20 bytes, the length is clamped to what the message holds |
| `OUTPUT_SELECT_TERMINAL` | `arg0` terminal | The next writes and clears are for that terminal; a number out of range is ignored |
| `OUTPUT_SHOW_TERMINAL` | `arg0` terminal | Puts that terminal on the screen by copying its image |
| `NONE` | none | Clears the screen and puts the cursor at the top left |

Until the grant arrives every drawing message is ignored. The screen is 80 by 25 cells with a fixed colour pair; when the last row is full the contents scroll up one row.

There is one 80x25 image with its cursor for each terminal (four); a write goes to the image of the selected terminal and is mirrored on the hardware only while that terminal is the one shown, so the text of one terminal never overwrites another.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main` and the receive loop that dispatches the message types |
| `src/vga.cpp` | `init`, `putChar`, `write`, `clear`, `newline`, `scroll`, `vga_entry` |
| `include/vga_driver/vga.hpp` | `cock::vga::VgaServer`, the geometry and colour constants, `VGAColor`; included as `<vga_driver/vga.hpp>` |
| `Makefile` | `NAME = vga_driver`, `SRCS` under `src/`, a short `LIBC_SRCS` list, `CONSOLE_CLIENT = no` |

## Build, run, test

```sh
make userland/drivers/vga_driver
make run
make run-headless
bash scripts/qemu_userland_test.sh
```

Use `HOST_TOOLCHAIN=1` without the cross compiler. `make run` shows the window; `make run-headless` has no window, so only the serial log is visible. The end to end script reads the video memory from the QEMU monitor (`xp` at the text buffer address) and requires the last shell lines on the screen, and `Screen mapped by PID` on the serial log. The syscall fuzzer sends this server random messages in its inbox storm (see `test/userland/fuzz`).

## Limits and known gaps

- No capability check on who may draw: any process can send these messages until the IPC capability of stage 9.
- No colour change request over IPC (`setColor` exists but no message calls it) and no scrollback.
- The mapping of the video memory is not returned to the frame allocator when the server dies, because device pages are not owned; the kernel then takes the screen back and writes to it directly.
- The server relies on the `OUTPUT_WRITE_STR` clamp found by the fuzzer; keep it when the message changes.

## Documentation

- [VGA display server](../../../documentation/userland/drivers/vga.md)
- [Console server](../../../documentation/userland/drivers/console.md)
- [Fuzzer findings](../../../documentation/userland/testing/fuzzer.md)
- [Capabilities](../../../documentation/core/capabilities.md)
