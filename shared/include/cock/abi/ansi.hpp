/**
 * @file ansi.hpp
 * @brief Reader of the ANSI escape sequences that choose the text color (`ESC [ n ; n m`). It does no I/O, so it is tested on the host.
 *
 * A program prints a color change as text, and every piece that shows the text (the screen driver) or must not show
 * it (the serial line) runs the same `SgrParser` over the bytes. The parser keeps the state between calls, so a sequence
 * may be cut at any byte by a chunk boundary. Only the color parameters of `SGR` are understood: reset, bold, the eight
 * foreground and background colors and the eight bright foreground colors. Every other sequence is read and dropped.
 */
#ifndef COCK_ABI_ANSI_HPP
#define COCK_ABI_ANSI_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::ansi {

/** First byte of every escape sequence. */
constexpr char ESCAPE = '\x1B';
/** Byte after `ESCAPE` that starts a control sequence. */
constexpr char SEQUENCE_START = '[';
/** Byte that separates the parameters of a sequence. */
constexpr char PARAMETER_SEPARATOR = ';';
/** Final byte of a sequence that sets the text style. */
constexpr char STYLE_FINAL = 'm';
/** Lowest final byte of a control sequence. */
constexpr char FINAL_FIRST = '@';
/** Highest final byte of a control sequence. */
constexpr char FINAL_LAST = '~';
/** First digit character of a parameter. */
constexpr char DIGIT_FIRST = '0';
/** Last digit character of a parameter. */
constexpr char DIGIT_LAST = '9';
/** Radix of the numbers in the parameters. */
constexpr uint32_t DECIMAL_RADIX = 10;
/** Most parameters of one sequence that are kept; the rest are dropped. */
constexpr size_t MAX_PARAMETERS = 8;
/** Largest value of one parameter; a bigger number stays at this value. */
constexpr uint32_t MAX_PARAMETER_VALUE = 999;

/** Value of `Style::foreground` and `Style::background` for the color the screen starts with. */
constexpr uint8_t DEFAULT_COLOR = 0xFF;
/** Number of colors in the normal range; the bright foregrounds follow them. */
constexpr uint8_t NORMAL_COLOR_COUNT = 8;

/** Parameter that resets every attribute. */
constexpr uint32_t RESET = 0;
/** Parameter that turns bold on. */
constexpr uint32_t BOLD = 1;
/** Parameter that turns bold off. */
constexpr uint32_t NORMAL_INTENSITY = 22;
/** First parameter of the normal foreground colors (black); the next seven follow. */
constexpr uint32_t FOREGROUND_FIRST = 30;
/** Parameter that restores the default foreground. */
constexpr uint32_t FOREGROUND_DEFAULT = 39;
/** First parameter of the normal background colors (black); the next seven follow. */
constexpr uint32_t BACKGROUND_FIRST = 40;
/** Parameter that restores the default background. */
constexpr uint32_t BACKGROUND_DEFAULT = 49;
/** First parameter of the bright foreground colors (dark gray); the next seven follow. */
constexpr uint32_t BRIGHT_FOREGROUND_FIRST = 90;
/** First parameter of the bright background colors; they are shown as the normal ones. */
constexpr uint32_t BRIGHT_BACKGROUND_FIRST = 100;

/** The ANSI color numbers, in the order of the parameters 30 to 37. */
enum class Color : uint8_t {
	BLACK = 0,
	RED = 1,
	GREEN = 2,
	YELLOW = 3,
	BLUE = 4,
	MAGENTA = 5,
	CYAN = 6,
	WHITE = 7,
};

/** What a sequence chose for the text that follows. */
struct Style {
	/** Foreground: 0 to 7 normal, 8 to 15 bright, or `DEFAULT_COLOR`. */
	uint8_t foreground = DEFAULT_COLOR;
	/** Background: 0 to 7, or `DEFAULT_COLOR`. */
	uint8_t background = DEFAULT_COLOR;
	/** True after `BOLD`: the normal foregrounds are shown bright. */
	bool bold = false;
};

/** What `SgrParser::feed` tells the caller to do with the byte. */
enum class Result {
	/** The byte is text: show it. */
	PRINT,
	/** The byte belongs to a sequence: show nothing. */
	CONSUMED,
	/** The byte ended a sequence that changed the style: show nothing and read `style`. */
	STYLE_CHANGED,
};

/** Reader of escape sequences with the style they chose. Not thread safe. */
class SgrParser {
  public:
	/**
	 * @brief Reads one byte of the text.
	 *
	 * A sequence is `ESCAPE`, `SEQUENCE_START`, numbers separated by `PARAMETER_SEPARATOR` and a final byte between
	 * `FINAL_FIRST` and `FINAL_LAST`. A sequence with the final byte `STYLE_FINAL` changes the style; with no number it
	 * resets it. Any other final byte ends a sequence that is dropped. A byte that cannot belong to a sequence (a newline
	 * in the middle of it, or anything but `SEQUENCE_START` after `ESCAPE`) cancels it and is returned as text, so a
	 * damaged sequence never swallows more than the bytes of the sequence itself.
	 * @param byte The next byte.
	 * @return What to do with the byte.
	 */
	Result feed(char byte) {
		switch (state) {
		case State::TEXT:
			if (byte != ESCAPE) return Result::PRINT;
			state = State::ESCAPED;
			return Result::CONSUMED;
		case State::ESCAPED:
			if (byte != SEQUENCE_START) return cancel();
			begin();
			state = State::PARAMETERS;
			return Result::CONSUMED;
		case State::PARAMETERS:
			return readParameter(byte);
		}
		return Result::PRINT;
	}

	/**
	 * @brief Gives the style the sequences read so far chose.
	 * @return The style; the default one before any sequence.
	 */
	const Style &style() const { return current; }

  private:
	enum class State : uint8_t { TEXT, ESCAPED, PARAMETERS };

	Result cancel() {
		state = State::TEXT;
		return Result::PRINT;
	}

	void begin() {
		count = 0;
		value = 0;
		hasValue = false;
	}

	void closeParameter() {
		if (count < MAX_PARAMETERS) parameters[count++] = hasValue ? value : RESET;
		value = 0;
		hasValue = false;
	}

	Result readParameter(char byte) {
		if (byte >= DIGIT_FIRST && byte <= DIGIT_LAST) {
			value = value * DECIMAL_RADIX + static_cast<uint32_t>(byte - DIGIT_FIRST);
			if (value > MAX_PARAMETER_VALUE) value = MAX_PARAMETER_VALUE;
			hasValue = true;
			return Result::CONSUMED;
		}
		if (byte == PARAMETER_SEPARATOR) {
			closeParameter();
			return Result::CONSUMED;
		}
		if (byte < FINAL_FIRST || byte > FINAL_LAST) return cancel();

		closeParameter();
		state = State::TEXT;
		if (byte != STYLE_FINAL) return Result::CONSUMED;

		for (size_t index = 0; index < count; index++)
			apply(parameters[index]);
		return Result::STYLE_CHANGED;
	}

	void apply(uint32_t parameter) {
		if (parameter == RESET) {
			current = Style{};
		} else if (parameter == BOLD) {
			current.bold = true;
		} else if (parameter == NORMAL_INTENSITY) {
			current.bold = false;
		} else if (parameter >= FOREGROUND_FIRST && parameter < FOREGROUND_FIRST + NORMAL_COLOR_COUNT) {
			current.foreground = static_cast<uint8_t>(parameter - FOREGROUND_FIRST);
		} else if (parameter == FOREGROUND_DEFAULT) {
			current.foreground = DEFAULT_COLOR;
		} else if (parameter >= BACKGROUND_FIRST && parameter < BACKGROUND_FIRST + NORMAL_COLOR_COUNT) {
			current.background = static_cast<uint8_t>(parameter - BACKGROUND_FIRST);
		} else if (parameter == BACKGROUND_DEFAULT) {
			current.background = DEFAULT_COLOR;
		} else if (parameter >= BRIGHT_FOREGROUND_FIRST && parameter < BRIGHT_FOREGROUND_FIRST + NORMAL_COLOR_COUNT) {
			current.foreground = static_cast<uint8_t>(parameter - BRIGHT_FOREGROUND_FIRST + NORMAL_COLOR_COUNT);
		} else if (parameter >= BRIGHT_BACKGROUND_FIRST && parameter < BRIGHT_BACKGROUND_FIRST + NORMAL_COLOR_COUNT) {
			current.background = static_cast<uint8_t>(parameter - BRIGHT_BACKGROUND_FIRST);
		}
	}

	State state = State::TEXT;
	Style current;
	uint32_t parameters[MAX_PARAMETERS] = {};
	size_t count = 0;
	uint32_t value = 0;
	bool hasValue = false;
};

} // namespace cock::abi::ansi

#endif // !COCK_ABI_ANSI_HPP
