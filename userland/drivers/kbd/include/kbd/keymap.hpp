/**
 * @file keymap.hpp
 * @brief US layout scancode (set 1) to ASCII translation for the keyboard driver.
 *
 * Pure table lookup with no system calls and no I/O: the driver reads scancodes from
 * the controller and feeds them to `Decoder`.
 */
#ifndef KBD_KEYMAP_HPP
#define KBD_KEYMAP_HPP

#include <stddef.h>
#include <cock/abi/console.hpp>
#include <stdint.h>

namespace cock::kbd {

/** Number of scancodes covered by the tables (codes 0x00 to 0x39). */
constexpr size_t KEYMAP_SIZE = 0x3A;
/** Bit set in a scancode when the key is released. */
constexpr uint8_t RELEASE_BIT = 0x80;
/** Prefix byte of extended keys; the key that follows is ignored. */
constexpr uint8_t EXTENDED_PREFIX = 0xE0;
/** Scancode of the left shift key. */
constexpr uint8_t LEFT_SHIFT = 0x2A;
/** Scancode of the right shift key. */
constexpr uint8_t RIGHT_SHIFT = 0x36;
/** Scancode of the caps lock key. */
constexpr uint8_t CAPS_LOCK = 0x3A;
/** Scancode of the left alt key. */
constexpr uint8_t LEFT_ALT = 0x38;
/** Scancode of F1; the next ones are F2, F3 and F4. */
constexpr uint8_t FIRST_FUNCTION_KEY = 0x3B;
/** Value of `takeSwitch` when no terminal was asked for. */
constexpr uint32_t NO_SWITCH = 0;
/** Value returned for scancodes that produce no character. */
constexpr char NO_CHARACTER = '\0';

/** Characters without shift, indexed by scancode. */
constexpr char LOWERCASE[KEYMAP_SIZE] = {
	NO_CHARACTER, NO_CHARACTER, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', '\b', '\t',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', '\n', NO_CHARACTER, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', NO_CHARACTER, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', NO_CHARACTER, '*',
	NO_CHARACTER, ' '};

/** Characters with shift held, indexed by scancode. */
constexpr char UPPERCASE[KEYMAP_SIZE] = {
	NO_CHARACTER, NO_CHARACTER, '!', '@', '#', '$', '%', '^',
	'&', '*', '(', ')', '_', '+', '\b', '\t',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '{', '}', '\n', NO_CHARACTER, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~', NO_CHARACTER, '|', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?', NO_CHARACTER, '*',
	NO_CHARACTER, ' '};

/** Stateful scancode decoder that tracks shift, caps lock and extended prefixes. */
class Decoder {
  public:
	/**
	 * @brief Processes one scancode byte.
	 *
	 * Shift and caps lock combine (shift XOR caps selects the upper table). Key releases,
	 * extended keys (the byte after `EXTENDED_PREFIX`) and codes outside the table give no
	 * character. Never blocks.
	 * @param scancode Raw byte from the keyboard controller.
	 * @return The ASCII character, or `NO_CHARACTER` when the byte only changes state or
	 *         has no character.
	 */
	char feed(uint8_t scancode) {
		if (scancode == EXTENDED_PREFIX) {
			skipNext = true;
			return NO_CHARACTER;
		}
		if (skipNext) {
			skipNext = false;
			return NO_CHARACTER;
		}

		bool released = (scancode & RELEASE_BIT) != 0;
		uint8_t code = scancode & static_cast<uint8_t>(~RELEASE_BIT);

		if (code == LEFT_SHIFT || code == RIGHT_SHIFT) {
			shiftHeld = !released;
			return NO_CHARACTER;
		}
		if (code == LEFT_ALT) {
			altHeld = !released;
			return NO_CHARACTER;
		}
		if (!released && altHeld && code >= FIRST_FUNCTION_KEY &&
			code < FIRST_FUNCTION_KEY + cock::abi::console::TERMINAL_COUNT) {
			switchTo = static_cast<uint32_t>(code - FIRST_FUNCTION_KEY) + 1;
			return NO_CHARACTER;
		}
		if (code == CAPS_LOCK) {
			if (!released) capsLocked = !capsLocked;
			return NO_CHARACTER;
		}
		if (released || code >= KEYMAP_SIZE) return NO_CHARACTER;

		return (shiftHeld != capsLocked) ? UPPERCASE[code] : LOWERCASE[code];
	}

	/**
	 * @brief Takes the terminal that an Alt plus function key combination asked for.
	 *
	 * Alt+F1 asks for the first terminal, Alt+F2 for the second, and so on up to `TERMINAL_COUNT`.
	 * @return The terminal number plus one, or `NO_SWITCH` when nothing was asked since the last call.
	 */
	uint32_t takeSwitch() {
		uint32_t asked = switchTo;
		switchTo = NO_SWITCH;
		return asked;
	}

  private:
	/** true while the left alt key is down. */
	bool altHeld = false;
	/** Terminal asked for by the last Alt plus function key, plus one; `NO_SWITCH` when none. */
	uint32_t switchTo = NO_SWITCH;
	/** true while a shift key is down. */
	bool shiftHeld = false;
	/** true while caps lock is on. */
	bool capsLocked = false;
	/** true when the next byte belongs to an extended key and must be ignored. */
	bool skipNext = false;
};

} // namespace cock::kbd

#endif // !KBD_KEYMAP_HPP
