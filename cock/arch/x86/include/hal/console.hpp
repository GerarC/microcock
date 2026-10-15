#ifndef ARCH_X86_HAL_CONSOLE_HPP
#define ARCH_X86_HAL_CONSOLE_HPP

#include <cock/arch/x86/driver/vga.hpp>
#include <hal/serial.hpp>
#include <stddef.h>

namespace cock::core::hal {

inline constinit arch::x86::VGA console_screen;
inline bool console_screen_attached = true;

inline void console_init() {
	serial_init();
	console_screen.clear();
}

inline void console_write(const char *data, size_t size) {
	for (size_t idx = 0; idx < size; idx++)
		serial_write(data[idx]);
	if (console_screen_attached) console_screen.write(data, size);
}

inline void console_detach() { console_screen_attached = false; }

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_CONSOLE_HPP
