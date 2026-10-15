# Keyboard Driver

The PS/2 keyboard driver runs in Ring 3 as the program `kbd` (`userland/kbd`). The kernel has no keyboard code: it only routes the interrupt to the driver, and the driver does the rest. It is the reference for how a hardware driver is written in MicroCOCK.

## What it needs
A process starts with no access to the hardware, so `kbd` depends on what the root server gives it (see [capabilities](capabilities.md)):

| Capability | Why |
|------------|-----|
| `IO_PORTS`, port `0x60` | Read the scan code from the keyboard controller |
| `IRQ`, line 1 | Receive the keyboard interrupt |

`init` grants both and then sends `CAPABILITY_GRANTED` to `kbd`. The argument of that message is the process id of the display server, which is where the characters will go.

## Start-up
```cpp
extern "C" void _start() {
	uint32_t display = waitForCapabilities();

	if (!cock::user::request_ports(PS2_DATA_PORT, PS2_PORT_COUNT) ||
		!cock::user::irq_bind(KEYBOARD_IRQ_LINE)) {
		cock::user::write_text(FAILED_MESSAGE, sizeof(FAILED_MESSAGE) - 1);
		cock::user::exit_process(FAILED_STATUS);
	}
	cock::user::write_text(READY_MESSAGE, sizeof(READY_MESSAGE) - 1);
	...
}
```

1. It blocks on `ipc_recv` until a `CAPABILITY_GRANTED` message arrives, and takes the first one it receives.
2. It enables port `0x60` in its permission bitmap (`SYS_REQUEST_RESOURCE`) and binds IRQ 1 (`SYS_IRQ_BIND`). Either one is denied if `init` did not grant it, and then the driver prints `kbd: cannot bind the keyboard` and exits.
3. It prints `kbd: ready`. The end to end test waits for that line on the serial log.

## Main loop
```cpp
for (;;) {
	cock::user::ipc_recv(message);
	if (message.type != MessageType::IRQ_NOTIFICATION) continue;

	show(display, decoder.feed(cock::user::port_in8(PS2_DATA_PORT)));
	cock::user::irq_ack(KEYBOARD_IRQ_LINE);
}
```

A key press travels like this:

1. The keyboard controller raises IRQ 1.
2. The kernel **masks the line** and sends an `IRQ_NOTIFICATION` message to `kbd` (see [ipc](ipc.md#interrupts-as-messages)).
3. `kbd` wakes up, reads one byte from port `0x60` and gives it to the decoder.
4. If the byte produces a character, `kbd` sends a `VIDEO_WRITE_CHAR` message to the display server, which draws it.
5. `kbd` calls `SYS_IRQ_ACK`, and the kernel unmasks the line so the next key can arrive.

The line stays masked while the driver works, so a burst of keys never piles up interrupts in the kernel. The thread is blocked in `ipc_recv` between keys and uses no CPU. If the driver dies, the kernel masks and releases the line.

## The decoder
`userland/kbd/keymap.hpp` turns scan codes into characters. It is a small state machine with no I/O, so it can be tested on its own.

* **Scan code set 1**, which is what the controller produces by default.
* The high bit marks a key **release**; releases produce no character.
* **Shift** (left and right) is tracked while held. **Caps Lock** toggles on press. The upper case table is used when exactly one of them is active, so Shift with Caps Lock gives lower case.
* The `0xE0` prefix of the extended keys (arrows, right Control, the numeric keypad, and so on) is recognized and the key that follows is ignored.
* Two tables of `0x3A` entries, one for each case, map the codes of a US layout: letters, digits, punctuation, Enter (`\n`), Backspace (`\b`), Tab (`\t`) and Space. Keys without a character give `NO_CHARACTER` and are dropped.

```cpp
char feed(uint8_t scancode) {
	...
	return (shiftHeld != capsLocked) ? UPPERCASE[code] : LOWERCASE[code];
}
```

## Testing
`scripts/qemu_userland_test.sh` types `a`, `b`, `Shift+c` and `d` through the QEMU monitor. The keys go through the whole path above (controller, IRQ 1, `kbd`, IPC, `vga_driver`) and the script reads the video memory and requires `abCd` on the screen, together with `kbd: ready` on the serial log and no exception.

## Current limits
* US layout only, and no key repeat handling beyond what the controller sends.
* The characters go to the display server only. There is no input buffer, so `SYS_READ` has nothing to read from; that needs a console server.
* The driver does not read the controller status port (`0x64`); it assumes that every interrupt carries one byte in the data port.
* The extended keys (arrows, function keys, the numeric keypad) do not produce anything.
