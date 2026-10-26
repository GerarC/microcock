#include <date/format.hpp>

namespace cock::date {

namespace {

constexpr char ZERO = '0';
constexpr char DATE_SEPARATOR = '-';
constexpr char TIME_SEPARATOR = ':';
constexpr char DATE_TIME_SEPARATOR = ' ';
constexpr uint32_t RADIX = 10;
constexpr size_t YEAR_DIGITS = 4;
constexpr size_t PAIR_DIGITS = 2;

size_t put(char *out, size_t at, uint32_t value, size_t digits) {
	for (size_t index = digits; index > 0; index--) {
		out[at + index - 1] = static_cast<char>(ZERO + value % RADIX);
		value /= RADIX;
	}
	return at + digits;
}

} // namespace

void formatDateTime(const cock::abi::datetime::DateTime &time, char (&out)[TEXT_BYTES]) {
	size_t at = put(out, 0, time.year, YEAR_DIGITS);
	out[at++] = DATE_SEPARATOR;
	at = put(out, at, time.month, PAIR_DIGITS);
	out[at++] = DATE_SEPARATOR;
	at = put(out, at, time.day, PAIR_DIGITS);
	out[at++] = DATE_TIME_SEPARATOR;
	at = put(out, at, time.hour, PAIR_DIGITS);
	out[at++] = TIME_SEPARATOR;
	at = put(out, at, time.minute, PAIR_DIGITS);
	out[at++] = TIME_SEPARATOR;
	at = put(out, at, time.second, PAIR_DIGITS);
	out[at] = '\0';
}

} // namespace cock::date
