# kbd

The PS/2 keyboard driver. It runs in Ring 3, takes the interrupt of the keyboard controller, decodes scan codes into characters and sends them to the `tty` server. The kernel has no keyboard code: it only routes IRQ 1 to this process. It is the reference for writing a hardware driver in MicroCOCK.

## Where it sits

```text
keyboard controller --IRQ--> kernel --IRQ_NOTIFICATION--> kbd --CONSOLE_KEY--> tty
```

| Need | Value in `initrd/etc/manifest.conf` |
|------|---------------------------------|
| `IO_PORTS` | `io:0x60+1`, the data port of the controller |
| `IRQ` | `irq:1` |
| Terminal | `ipc=tty`: the driver connects to the `tty` server through the name server and sends it the characters |

The manifest line is `kbd path=drivers/kbd` with those grants. `rs` grants them and then sends `CAPABILITY_GRANTED`; The message carries nothing the driver needs. The test manifest adds nothing for `kbd`. The program opts out of the console client (`CONSOLE_CLIENT = no`): the input path must not depend on the server it feeds.

## Protocol

| Message | Direction | Content |
|---------|-----------|---------|
| `CAPABILITY_GRANTED` | `rs` to `kbd` | `arg0` is the display (console) pid; the first one received is taken |
| `IRQ_NOTIFICATION` | kernel to `kbd` | The line fired; only the kernel can produce this type, so the type is the whole check |
| `CONSOLE_KEY` | `kbd` to `tty` | `arg0` is the typed character |

Message layout: `shared/include/cock/ipc/message.hpp`. Per key the driver reads one byte from the data port, feeds the decoder, sends the character if there is one and calls `SYS_IRQ_ACK`. The kernel keeps the line masked until the acknowledge, so a burst of keys never piles up interrupts. If a capability is missing the driver prints `kbd: cannot bind the keyboard` and exits; on success it prints `kbd: ready`.

## Decoder

`keymap.hpp` is a small state machine with no I/O: scan code set 1, the high bit is a release (no character), Shift and Caps Lock are tracked (`Shift` with `Caps Lock` gives lower case), the `0xE0` prefix is recognized and the key after it is ignored. Two tables of `KEYMAP_SIZE` entries map a US layout; keys without a character give `NO_CHARACTER` and are dropped.

## Terminal switch

The decoder tracks both Alt keys (the right one is the left one's scan code after the `0xE0` prefix). Alt+F1 to Alt+F4 give no character: `Decoder::takeSwitch` returns the terminal plus one and `sendSwitch` sends `tty` a `CONSOLE_KEY` with `arg1` set to it.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: the grant, the controller, the receive loop that decodes and sends each key |
| `src/grant.cpp`, `include/kbd/grant.hpp` | `waitForCapabilities`: blocks until `CAPABILITY_GRANTED` and returns the console server |
| `src/controller.cpp`, `include/kbd/controller.hpp` | The PS/2 controller: `openController` (port `0x60` and IRQ 1), `isInterrupt`, `readScancode`, `acknowledge` |
| `src/console_link.cpp`, `include/kbd/console_link.hpp` | `sendKey`: sends a decoded key to `tty` as `CONSOLE_KEY` |
| `include/kbd/keymap.hpp` | `cock::kbd::Decoder`, the two layout tables and the scan code constants; header only, with no I/O |
| `Makefile` | `NAME = kbd`, `SRCS` under `src/`, `CONSOLE_CLIENT = no` |

## Build, run, test

```sh
make userland/drivers/kbd
make run
bash scripts/qemu_userland_test.sh
```

Add `HOST_TOOLCHAIN=1` without the cross compiler. The userland test sends keys with the QEMU monitor (`sendkey`), so each shell command crosses controller, IRQ 1, `kbd`, `tty`, `console` and `vga_driver`; it requires `kbd: ready` on the serial log and the typed line on the screen. There is no host test for the decoder yet.

## Limits and known gaps

- US layout only; no key repeat handling beyond what the controller sends.
- The status port (`0x64`) is not read: the driver assumes each interrupt carries one data byte.
- Extended keys (arrows, function keys, numeric keypad) produce nothing.
- The driver keeps no buffer; the `tty` server owns the input line.

## Documentation

- [Keyboard driver](../../../documentation/userland/drivers/keyboard.md)
- [Console server](../../../documentation/userland/drivers/console.md)
- [Capabilities](../../../documentation/core/capabilities.md)
- [IPC and interrupts as messages](../../../documentation/core/ipc.md)
