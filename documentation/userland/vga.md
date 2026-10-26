# VGA Display Drivers

Due to the microkernel architecture, MicroCOCK utilizes two distinct implementations of the VGA text-mode driver during its lifecycle.

## Kernel console
Provides the text output that the kernel itself uses: the boot messages, the log and the panic message. The HAL (`hal/console.hpp`) sends every byte to the **serial port** (COM1) and, while it is attached, to the VGA text buffer, which the boot code maps at `0xC00B8000`.

```cpp
inline void console_write(const char *data, size_t size) {
	for (size_t idx = 0; idx < size; idx++)
		serial_write(data[idx]);
	if (console_screen_attached) console_screen.write(data, size);
}
```

`VGA` is a final class with no virtual functions and a `constexpr` constructor, so the screen object is initialized at compile time and every call is direct. The serial output is what the test scripts read, and it keeps working when the screen belongs to the userland server.

## Userland VGA Server
The fully isolated Ring 3 display server handles standard output for the operating system. The kernel does not know the driver by name: the first process that maps the screen memory (`0xB8000`, with a `PHYSICAL_MEMORY` capability) makes the kernel detach its console from the screen and remember that process as the destination of the output of `sys_write`. Without that capability the map is refused, so no other program can take the screen. When the inbox of the display server is full, `sys_write` restarts the call so the server can drain it; after 1000 restarts it writes the text to the kernel console instead (`WRITE_RETRY_LIMIT`), so a stuck display server cannot keep a writer spinning forever.

The server cannot touch the hardware until it is allowed to. It waits for the `CAPABILITY_GRANTED` message of the root server, which has given it a `PHYSICAL_MEMORY` capability over the legacy video window (`0xA0000` to `0xBFFFF`). Only then does it use the `mmap` syscall to map the physical text buffer (`0xB8000`) into its own virtual address space at `0xA0000000`. The kernel checks that the capability covers the range and that the range is not RAM. The kernel only writes cells and never programs the CRT controller, so the hardware cursor would stay where the boot loader left it; the driver also holds an `io` capability over the two ports `0x3D4` and `0x3D5` and, after each write, clear or terminal switch, sets the cursor location registers (`0x0E` high byte, `0x0F` low byte) to the cursor of the terminal on the screen.

```cpp
bool VgaServer::init() {
	void *mapped = mmap((void *)VGA_VIRTUAL_TARGET, PAGE_SIZE,
						PROT_READ | PROT_WRITE,
						MAP_PHYS | MAP_FIXED, -1,
						VGA_PHYSICAL_ADDRESS);
	if (mapped == MAP_FAILED) {
		buffer = nullptr;
		return false;
	}
	buffer = static_cast<volatile uint16_t *>(mapped);
	return true;
}
```

After that it loops on `sys_ipc_recv`. The kernel routes text data to it via IPC messages containing types like `OUTPUT_WRITE_CHAR` or `OUTPUT_WRITE_STR`, and so does any other program, such as the keyboard driver.

```cpp
extern "C" int main() {
	bool ready = false;

	Message msg = {};
	while (true) {
		cock::user::ipc_recv(msg);
		switch (msg.type) {
			case MessageType::CAPABILITY_GRANTED:
				if (!ready) ready = VgaServer::init();
				break;
			case MessageType::OUTPUT_WRITE_CHAR:
				if (ready) VgaServer::putChar(static_cast<char>(msg.payload.args.arg0));
				break;
			case MessageType::OUTPUT_WRITE_STR: {
				if (!ready) break;
				size_t length = msg.payload.args.arg0;
				const char *str_chunk =
					reinterpret_cast<const char *>(&msg.payload.args.arg1);
				VgaServer::write(str_chunk, length);
				break;
			}
			case MessageType::NONE:
				if (ready) VgaServer::clear();
				break;
			default:
				break;
		}
	}
}
```

### When the server is not there
`OUTPUT_WRITE_CHAR` also understands the backspace character: it moves the cursor one column back and blanks the cell (the console server uses it to echo an erased key); at column 0 it does nothing. The console server draws its output with `OUTPUT_WRITE_STR` too. The driver keeps one image of the screen for each of the four terminals: `OUTPUT_SELECT_TERMINAL` (`arg0` is the terminal) chooses which one the following messages write to, and `OUTPUT_SHOW_TERMINAL` copies one to the hardware; only the terminal on the screen is mirrored on the hardware while it is written. See [tty](tty.md#terminals).

`sys_write` takes at most 128 bytes per call and sends the text to the server in chunks of 20 bytes. If the server has died, the kernel notices, takes the screen back and writes to it directly, so the system keeps printing. Its mapping of the video memory is not returned to the frame allocator when the server dies, because device pages are marked as not owned.
