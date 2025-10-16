#include "keyboard.hpp"
#include "../interrupt/irq/irq.hpp"
#include <stdio.h>

namespace cock::arch::x86 {

const uint32_t UNKNOWN = 0xFFFFFFFF;
const uint32_t ESC = 0xFFFFFFFF - 1;
const uint32_t CTRL = 0xFFFFFFFF - 2;
const uint32_t LSHFT = 0xFFFFFFFF - 3;
const uint32_t RSHFT = 0xFFFFFFFF - 4;
const uint32_t ALT = 0xFFFFFFFF - 5;
const uint32_t F1 = 0xFFFFFFFF - 6;
const uint32_t F2 = 0xFFFFFFFF - 7;
const uint32_t F3 = 0xFFFFFFFF - 8;
const uint32_t F4 = 0xFFFFFFFF - 9;
const uint32_t F5 = 0xFFFFFFFF - 10;
const uint32_t F6 = 0xFFFFFFFF - 11;
const uint32_t F7 = 0xFFFFFFFF - 12;
const uint32_t F8 = 0xFFFFFFFF - 13;
const uint32_t F9 = 0xFFFFFFFF - 14;
const uint32_t F10 = 0xFFFFFFFF - 15;
const uint32_t F11 = 0xFFFFFFFF - 16;
const uint32_t F12 = 0xFFFFFFFF - 17;
const uint32_t SCRLCK = 0xFFFFFFFF - 18;
const uint32_t HOME = 0xFFFFFFFF - 19;
const uint32_t UP = 0xFFFFFFFF - 20;
const uint32_t LEFT = 0xFFFFFFFF - 21;
const uint32_t RIGHT = 0xFFFFFFFF - 22;
const uint32_t DOWN = 0xFFFFFFFF - 23;
const uint32_t PGUP = 0xFFFFFFFF - 24;
const uint32_t PGDOWN = 0xFFFFFFFF - 25;
const uint32_t END = 0xFFFFFFFF - 26;
const uint32_t INS = 0xFFFFFFFF - 27;
const uint32_t DEL = 0xFFFFFFFF - 28;
const uint32_t CAPS = 0xFFFFFFFF - 29;
const uint32_t NONE = 0xFFFFFFFF - 30;
const uint32_t ALTGR = 0xFFFFFFFF - 31;
const uint32_t NUMLCK = 0xFFFFFFFF - 32;

const uint32_t lowercase[128] = {
	UNKNOWN, ESC,	  '1',	   '2',		'3',	 '4',	  '5',	   '6',
	'7',	 '8',	  '9',	   '0',		'-',	 '=',	  '\b',	   '\t',
	'q',	 'w',	  'e',	   'r',		't',	 'y',	  'u',	   'i',
	'o',	 'p',	  '[',	   ']',		'\n',	 CTRL,	  'a',	   's',
	'd',	 'f',	  'g',	   'h',		'j',	 'k',	  'l',	   ';',
	'\'',	 '`',	  LSHFT,   '\\',	'z',	 'x',	  'c',	   'v',
	'b',	 'n',	  'm',	   ',',		'.',	 '/',	  RSHFT,   '*',
	ALT,	 ' ',	  CAPS,	   F1,		F2,		 F3,	  F4,	   F5,
	F6,		 F7,	  F8,	   F9,		F10,	 NUMLCK,  SCRLCK,  HOME,
	UP,		 PGUP,	  '-',	   LEFT,	UNKNOWN, RIGHT,	  '+',	   END,
	DOWN,	 PGDOWN,  INS,	   DEL,		UNKNOWN, UNKNOWN, UNKNOWN, F11,
	F12,	 UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
	UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
	UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
	UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
	UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN};

const uint32_t uppercase[128] = {
	UNKNOWN, ESC,	  '!',	   '@',		'#',	 '$',	  '%',	   '^',
	'&',	 '*',	  '(',	   ')',		'_',	 '+',	  '\b',	   '\t',
	'Q',	 'W',	  'E',	   'R',		'T',	 'Y',	  'U',	   'I',
	'O',	 'P',	  '{',	   '}',		'\n',	 CTRL,	  'A',	   'S',
	'D',	 'F',	  'G',	   'H',		'J',	 'K',	  'L',	   ':',
	'"',	 '~',	  LSHFT,   '|',		'Z',	 'X',	  'C',	   'V',
	'B',	 'N',	  'M',	   '<',		'>',	 '?',	  RSHFT,   '*',
	ALT,	 ' ',	  CAPS,	   F1,		F2,		 F3,	  F4,	   F5,
	F6,		 F7,	  F8,	   F9,		F10,	 NUMLCK,  SCRLCK,  HOME,
	UP,		 PGUP,	  '-',	   LEFT,	UNKNOWN, RIGHT,	  '+',	   END,
	DOWN,	 PGDOWN,  INS,	   DEL,		UNKNOWN, UNKNOWN, UNKNOWN, F11,
	F12,	 UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
	UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
	UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
	UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN,
	UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN, UNKNOWN};

constexpr bool DEFAULT_CAPS_ON = false;
constexpr bool DEFAULT_CAPS_LOCK = false;

using utils::in_port_b;

bool Keyboard::caps_on = DEFAULT_CAPS_ON;
bool Keyboard::caps_lock = DEFAULT_CAPS_LOCK;

void Keyboard::init() {
	caps_on = DEFAULT_CAPS_ON;
	caps_lock = DEFAULT_CAPS_LOCK;
	irq_install_handler(KEYBOARD_IRQ_INDEX, Keyboard::handler);
}

static KeyCode get_key_code(uint8_t raw) {
	return static_cast<KeyCode>(raw & 0x7F);
}

/**
 * Returns true if pressed false otherwise
 * */
static bool pressed(uint8_t raw) { return (raw & 0x80) == 0; }

void Keyboard::handler(InterruptRegisters *regs) {
	(void)regs;
	uint8_t raw = in_port_b(static_cast<uint16_t>(PS2Port::DATA));
	KeyCode code = get_key_code(raw);
	uint8_t is_pressed = pressed(raw);

	switch (code) {
		case KeyCode::LeftShift:
		case KeyCode::RightShift:
			if (is_pressed) caps_on = true;
			else caps_on = false;
			break;
		case KeyCode::CapsLock:
			if (is_pressed) caps_lock = !caps_lock;
			break;
		case KeyCode::Escape:
		case KeyCode::LeftCtrl:
		case KeyCode::LeftAlt:
		case KeyCode::F1:
		case KeyCode::F2:
		case KeyCode::F3:
		case KeyCode::F4:
		case KeyCode::F5:
		case KeyCode::F6:
		case KeyCode::F7:
		case KeyCode::F8:
		case KeyCode::F9:
		case KeyCode::F10:
		case KeyCode::F11:
		case KeyCode::F12:
			break;
		default:
			if (is_pressed) {
				if (caps_lock || caps_on)
					putchar(uppercase[static_cast<size_t>(code)]);
				else putchar(lowercase[static_cast<size_t>(code)]);
			}
			break;
	}
}

} // namespace cock::arch::x86
