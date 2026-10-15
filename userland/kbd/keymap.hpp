#ifndef KBD_KEYMAP_HPP
#define KBD_KEYMAP_HPP

#include <stddef.h>
#include <stdint.h>

namespace kbd {

constexpr size_t KEYMAP_SIZE = 0x3A;
constexpr uint8_t RELEASE_BIT = 0x80;
constexpr uint8_t EXTENDED_PREFIX = 0xE0;
constexpr uint8_t LEFT_SHIFT = 0x2A;
constexpr uint8_t RIGHT_SHIFT = 0x36;
constexpr uint8_t CAPS_LOCK = 0x3A;
constexpr char NO_CHARACTER = '\0';

constexpr char LOWERCASE[KEYMAP_SIZE] = {
	NO_CHARACTER, NO_CHARACTER, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', '\b', '\t',
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', '\n', NO_CHARACTER, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', NO_CHARACTER, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', NO_CHARACTER, '*',
	NO_CHARACTER, ' '};

constexpr char UPPERCASE[KEYMAP_SIZE] = {
	NO_CHARACTER, NO_CHARACTER, '!', '@', '#', '$', '%', '^',
	'&', '*', '(', ')', '_', '+', '\b', '\t',
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '{', '}', '\n', NO_CHARACTER, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~', NO_CHARACTER, '|', 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', '<', '>', '?', NO_CHARACTER, '*',
	NO_CHARACTER, ' '};

class Decoder {
  public:
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
		if (code == CAPS_LOCK) {
			if (!released) capsLocked = !capsLocked;
			return NO_CHARACTER;
		}
		if (released || code >= KEYMAP_SIZE) return NO_CHARACTER;

		return (shiftHeld != capsLocked) ? UPPERCASE[code] : LOWERCASE[code];
	}

  private:
	bool shiftHeld = false;
	bool capsLocked = false;
	bool skipNext = false;
};

} // namespace kbd

#endif // !KBD_KEYMAP_HPP
