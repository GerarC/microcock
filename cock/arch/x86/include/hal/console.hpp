#ifndef ARCH_X86_HAL_CONSOLE_HPP
#define ARCH_X86_HAL_CONSOLE_HPP

/**
 * @file
 * @brief HAL: kernel debug console, mirrored to the serial port and the VGA
 * text screen.
 *
 * Port requirement: provide console_init(), console_write(), console_detach()
 * and console_attach() with these signatures. The kernel logger and panic path
 * write only through them. A port may map them to any output device (UART,
 * framebuffer); detach and attach are used to hand the screen over to a Ring 3
 * display driver, so the log must keep flowing to a secondary sink if one
 * exists.
 */

#include <cock/arch/x86/driver/vga.hpp>
#include <hal/serial.hpp>
#include <stddef.h>

namespace cock::core::hal {

/** @brief Physical address of the memory that holds the screen text. */
inline constexpr uintptr_t CONSOLE_SCREEN_BASE = arch::x86::VGA::PHYSICAL_BUFFER;
/** @brief Size in bytes of the screen text memory (one 16 bit cell per character). */
inline constexpr size_t CONSOLE_SCREEN_SIZE =
	arch::x86::VGA::WIDTH * arch::x86::VGA::HEIGHT * sizeof(uint16_t);

/** @brief The kernel owned VGA text writer. */
inline constinit arch::x86::VGA console_screen;
/** @brief True while kernel output is also drawn on the VGA screen. */
inline bool console_screen_attached = true;

/**
 * @brief Brings up the console.
 *
 * Programs the COM1 UART (see serial_init()) and blanks the VGA screen. Must
 * run once, in Ring 0, before the first console_write().
 */
inline void console_init() {
	serial_init();
	console_screen.clear();
}

/**
 * @brief Writes bytes to the console.
 *
 * Every byte goes to COM1 (polling the line status register, so it blocks until
 * the UART accepts it); when the screen is attached the same bytes are also
 * drawn on VGA. Ring 0 only, not reentrant.
 *
 * @param data Pointer to at least `size` readable bytes.
 * @param size Number of bytes to write.
 */
inline void console_write(const char *data, size_t size) {
	for (size_t idx = 0; idx < size; idx++)
		serial_write(data[idx]);
	if (console_screen_attached) console_screen.write(data, size);
}

/**
 * @brief Stops drawing kernel output on the VGA screen (serial stays active).
 *
 * Used when a user mode driver takes over the display.
 */
inline void console_detach() { console_screen_attached = false; }

/**
 * @brief Resumes drawing kernel output on the VGA screen.
 *
 * Used as a fallback when the user mode display driver has died.
 */
inline void console_attach() { console_screen_attached = true; }

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_CONSOLE_HPP
