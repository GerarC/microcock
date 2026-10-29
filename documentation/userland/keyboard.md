# Keyboard Driver

The PS/2 keyboard driver runs in Ring 3 as the program `kbd` (`userland/drivers/kbd`). The kernel has no keyboard code: it only routes the interrupt to the driver, and the driver does the rest. It is the reference for how a hardware driver is written in MicroCOCK.

## What it needs
A process starts with no access to the hardware, so `kbd` depends on what the root server gives it (see [capabilities](../core/capabilities.md)):

| Capability | Why |
|------------|-----|
| `IO_PORTS`, port `0x60` | Read the scan code from the keyboard controller |
| `IRQ`, line 1 | Receive the keyboard interrupt |

`rs` grants both and then sends `CAPABILITY_GRANTED` to `kbd`. The argument of that message is the process id of the display server, which is where the characters will go.

## Start-up
```cpp
extern "C" int main() {
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

1. It blocks on `ipc_recv` until a `CAPABILITY_GRANTED` message arrives, and takes the first one it receives. The kernel only lets a process that holds grant authority send that type, so the message can only come from the root server.
2. It enables port `0x60` in its permission bitmap (`SYS_REQUEST_RESOURCE`) and binds IRQ 1 (`SYS_IRQ_BIND`). Either one is denied if `rs` did not grant it, and then the driver prints `kbd: cannot bind the keyboard` and exits.
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
2. The kernel **masks the line** and raises a pending notification on `kbd`, which `ipc_recv` returns as an `IRQ_NOTIFICATION` message (see [ipc](../core/ipc.md#interrupts-as-messages)).
3. `kbd` wakes up, reads one byte from port `0x60` and gives it to the decoder.
4. If the byte produces a character, `kbd` sends a `CONSOLE_KEY` message to the console server, which edits the line, echoes it on the display server and keeps it for readers (see [console](console.md)).
5. `kbd` calls `SYS_IRQ_ACK`, and the kernel unmasks the line so the next key can arrive.

The line stays masked while the driver works, so a burst of keys never piles up interrupts in the kernel. The thread is blocked in `ipc_recv` between keys and uses no CPU. Because the notification does not use the inbox, another program that fills the inbox of `kbd` with messages cannot make it miss a key. If the driver dies, the kernel masks and releases the line.

## The decoder
`userland/drivers/kbd/include/kbd/keymap.hpp` turns scan codes into characters. It is a small state machine with no I/O, so it can be tested on its own.

* **Scan code set 1**, which is what the controller produces by default.
* The high bit marks a key **release**; releases produce no character.
* **Shift** (left and right) is tracked while held. **Caps Lock** toggles on press. The upper case table is used when exactly one of them is active, so Shift with Caps Lock gives lower case.
* The `0xE0` prefix of the extended keys (arrows, right Control, the numeric keypad, and so on) is recognized. Of the key that follows only the pressed Up and Down arrows count: they give `KEY_UP` and `KEY_DOWN`, which `tty` uses to recall lines (see [tty](tty.md)); every other extended key and every release is ignored.
* Two tables of `0x3A` entries, one for each case, map the codes of a US layout: letters, digits, punctuation, Enter (`\n`), Backspace (`\b`), Tab (`\t`) and Space. Keys without a character give `NO_CHARACTER` and are dropped.

```cpp
char feed(uint8_t scancode) {
	...
	return (shiftHeld != capsLocked) ? UPPERCASE[code] : LOWERCASE[code];
}
```

## Testing
`scripts/qemu_userland_test.sh` types shell commands through the QEMU monitor (`sendkey`, one key at a time, with `ret` after each line). The keys go through the whole path above (controller, IRQ 1, `kbd`, IPC, `console`, `vga_driver`) and the shell reads each line with `read(0)`; the script reads the video memory and requires the last typed line and its output on the screen, together with `kbd: ready` and the output of the commands on the serial log (see [shell](shell.md)) and no exception.

## Current limits
* US layout only, and no key repeat handling beyond what the controller sends.
* The driver keeps no buffer: the characters go to the console server, which owns the input line. `SYS_READ` never returns input, because the kernel has no input queue: it validates the buffer and reports end of input, and programs read the keyboard through the console server (see [console](console.md)).
* The driver does not read the controller status port (`0x64`); it assumes that every interrupt carries one byte in the data port.
* The extended keys other than Up and Down (Left, Right, Home, End, the numeric keypad) do not produce anything, and the function keys only switch terminals with Alt.
