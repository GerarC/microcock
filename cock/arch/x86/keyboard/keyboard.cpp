#include "cock/utils/logger.hpp"
#include <cock/arch/x86/interrupt/irq/irq.hpp>
#include <cock/arch/x86/keyboard/keyboard.hpp>
#include <stdio.h>

namespace cock::arch::x86 {

using cock::utils::Logger;
using utils::in_port_b;

constexpr size_t ASCII_SIZE = 0x80;

constexpr uint8_t CODE_MASK = 0x7F;
constexpr uint8_t PRESSED_MASK = 0x80;
constexpr uint8_t IS_PRESSED = 0x0;

constexpr bool DEFAULT_CAPS_ON = false;
constexpr bool DEFAULT_CAPS_LOCK = false;

constexpr uint32_t UNKNOWN = 0xFFFFFFFF;
constexpr uint32_t ESC = 0xFFFFFFFF - 1;
constexpr uint32_t CTRL = 0xFFFFFFFF - 2;
constexpr uint32_t LSHFT = 0xFFFFFFFF - 3;
constexpr uint32_t RSHFT = 0xFFFFFFFF - 4;
constexpr uint32_t ALT = 0xFFFFFFFF - 5;
constexpr uint32_t F1 = 0xFFFFFFFF - 6;
constexpr uint32_t F2 = 0xFFFFFFFF - 7;
constexpr uint32_t F3 = 0xFFFFFFFF - 8;
constexpr uint32_t F4 = 0xFFFFFFFF - 9;
constexpr uint32_t F5 = 0xFFFFFFFF - 10;
constexpr uint32_t F6 = 0xFFFFFFFF - 11;
constexpr uint32_t F7 = 0xFFFFFFFF - 12;
constexpr uint32_t F8 = 0xFFFFFFFF - 13;
constexpr uint32_t F9 = 0xFFFFFFFF - 14;
constexpr uint32_t F10 = 0xFFFFFFFF - 15;
constexpr uint32_t F11 = 0xFFFFFFFF - 16;
constexpr uint32_t F12 = 0xFFFFFFFF - 17;
constexpr uint32_t SCRLCK = 0xFFFFFFFF - 18;
constexpr uint32_t HOME = 0xFFFFFFFF - 19;
constexpr uint32_t UP = 0xFFFFFFFF - 20;
constexpr uint32_t LEFT = 0xFFFFFFFF - 21;
constexpr uint32_t RIGHT = 0xFFFFFFFF - 22;
constexpr uint32_t DOWN = 0xFFFFFFFF - 23;
constexpr uint32_t PGUP = 0xFFFFFFFF - 24;
constexpr uint32_t PGDOWN = 0xFFFFFFFF - 25;
constexpr uint32_t END = 0xFFFFFFFF - 26;
constexpr uint32_t INS = 0xFFFFFFFF - 27;
constexpr uint32_t DEL = 0xFFFFFFFF - 28;
constexpr uint32_t CAPS = 0xFFFFFFFF - 29;
constexpr uint32_t NONE = 0xFFFFFFFF - 30;
constexpr uint32_t ALTGR = 0xFFFFFFFF - 31;
constexpr uint32_t NUMLCK = 0xFFFFFFFF - 32;

constexpr uint32_t lowercase[ASCII_SIZE] = {
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

constexpr uint32_t uppercase[ASCII_SIZE] = {
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

bool Keyboard::caps_on = DEFAULT_CAPS_ON;
bool Keyboard::caps_lock = DEFAULT_CAPS_LOCK;

void Keyboard::init() {
	caps_on = DEFAULT_CAPS_ON;
	caps_lock = DEFAULT_CAPS_LOCK;
	irq_install_handler(KEYBOARD_IRQ_INDEX, Keyboard::handler);
	Logger::trace("Keyboard Initialized");
}

/**
 * Retrieves a key from the PS/2 data port and get its code and if it's pressed
 * */
Key Keyboard::getKey() {
	uint8_t raw = in_port_b(static_cast<uint16_t>(PS2Port::DATA));
	return {.code = static_cast<KeyCode>(raw & CODE_MASK),
			.pressed = (raw & PRESSED_MASK) == IS_PRESSED};
}

/**
 * Function triggered by the IRQ, This retrieves and handles PS/2 keyboard
 * inputs
 * */
void Keyboard::handler(InterruptRegisters *regs) {
	(void)regs;
	Key key = Keyboard::getKey();

	switch (key.code) {
		case KeyCode::LeftShift:
		case KeyCode::RightShift:
			if (key.pressed) caps_on = true;
			else caps_on = false;
			break;
		case KeyCode::CapsLock:
			if (key.pressed) caps_lock = !caps_lock;
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
			bool upper = caps_lock ^ caps_on;
			putchar(
				(upper ? uppercase : lowercase)[static_cast<size_t>(key.code)]);
			break;
	}
}

} // namespace cock::arch::x86
