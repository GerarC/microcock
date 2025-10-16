#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP
#include <cock/arch/x86/utils/helpers.hpp>
#include <stdint.h>

namespace cock::arch::x86 {

using utils::InterruptRegisters;

enum class KeyCode : uint8_t {
	Escape = 0x01,
	Num1 = 0x02,
	Num2 = 0x03,
	Num3 = 0x04,
	Num4 = 0x05,
	Num5 = 0x06,
	Num6 = 0x07,
	Num7 = 0x08,
	Num8 = 0x09,
	Num9 = 0x0A,
	Num0 = 0x0B,
	Minus = 0x0C,
	Equal = 0x0D,
	Backspace = 0x0E,
	Tab = 0x0F,

	Q = 0x10,
	W = 0x11,
	E = 0x12,
	R = 0x13,
	T = 0x14,
	Y = 0x15,
	U = 0x16,
	I = 0x17,
	O = 0x18,
	P = 0x19,
	LeftBracket = 0x1A,
	RightBracket = 0x1B,
	Enter = 0x1C,
	LeftCtrl = 0x1D,

	A = 0x1E,
	S = 0x1F,
	D = 0x20,
	F = 0x21,
	G = 0x22,
	H = 0x23,
	J = 0x24,
	K = 0x25,
	L = 0x26,
	Semicolon = 0x27,
	Apostrophe = 0x28,
	Backtick = 0x29,
	LeftShift = 0x2A,
	Backslash = 0x2B,

	Z = 0x2C,
	X = 0x2D,
	C = 0x2E,
	V = 0x2F,
	B = 0x30,
	N = 0x31,
	M = 0x32,
	Comma = 0x33,
	Period = 0x34,
	Slash = 0x35,
	RightShift = 0x36,

	KeypadAsterisk = 0x37,
	LeftAlt = 0x38,
	Space = 0x39,
	CapsLock = 0x3A,

	F1 = 0x3B,
	F2 = 0x3C,
	F3 = 0x3D,
	F4 = 0x3E,
	F5 = 0x3F,
	F6 = 0x40,
	F7 = 0x41,
	F8 = 0x42,
	F9 = 0x43,
	F10 = 0x44,

	NumLock = 0x45,
	ScrollLock = 0x46,

	Keypad7 = 0x47,
	Keypad8 = 0x48,
	Keypad9 = 0x49,
	KeypadMinus = 0x4A,
	Keypad4 = 0x4B,
	Keypad5 = 0x4C,
	Keypad6 = 0x4D,
	KeypadPlus = 0x4E,
	Keypad1 = 0x4F,
	Keypad2 = 0x50,
	Keypad3 = 0x51,
	Keypad0 = 0x52,
	KeypadDot = 0x53,

	F11 = 0x57,
	F12 = 0x58,
};

enum class PS2Port : uint16_t {
	DATA = 0x60,
	STATUS = 0x64,
	COMMAND = 0x64,
};

typedef struct {
	KeyCode code;
	bool pressed;
} Key;

class Keyboard {
  public:
	void init();

  private:
	static bool caps_on;
	static bool caps_lock;
	static constexpr int KEYBOARD_IRQ_INDEX = 0x21;
	static void handler(InterruptRegisters *regs);
	static Key getKey();
};

} // namespace cock::arch::x86

#endif // !KEYBOARD_HPP
