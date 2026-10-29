/** @file
 * @brief Reading a number typed as an argument.
 */
#ifndef COCK_USER_NUMBER_HPP
#define COCK_USER_NUMBER_HPP

#include <stdint.h>

namespace cock::user::tool {

/** @brief Base of the numbers programs read from their arguments. */
constexpr uint32_t DECIMAL_BASE = 10;

/** @brief First decimal digit. */
constexpr char DIGIT_FIRST = '0';

/** @brief Last decimal digit. */
constexpr char DIGIT_LAST = '9';

/** @brief End of a string. */
constexpr char TERMINATOR = '\0';

/**
 * @brief Reads an unsigned decimal number.
 *
 * @param text Null terminated text made only of the digits `0` to `9`; at least one.
 * @param value Receives the number; left unchanged when the text is not valid.
 * @return True when the text is a number that fits in 32 bits; false for null, empty, any other character or an
 * overflow.
 */
inline bool parseUnsigned(const char *text, uint32_t &value) {
	if (text == nullptr || *text == TERMINATOR) return false;

	uint32_t result = 0;
	for (; *text != TERMINATOR; text++) {
		if (*text < DIGIT_FIRST || *text > DIGIT_LAST) return false;
		uint32_t digit = static_cast<uint32_t>(*text - DIGIT_FIRST);
		if (result > (UINT32_MAX - digit) / DECIMAL_BASE) return false;
		result = result * DECIMAL_BASE + digit;
	}
	value = result;
	return true;
}

} // namespace cock::user::tool

#endif // !COCK_USER_NUMBER_HPP
