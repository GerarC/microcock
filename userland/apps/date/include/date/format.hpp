/** @file
 * @brief The text of a date and time: `YYYY-MM-DD HH:MM:SS`.
 */
#ifndef DATE_FORMAT_HPP
#define DATE_FORMAT_HPP

#include <cock/abi/datetime.hpp>
#include <stddef.h>

namespace cock::date {

/** @brief Size of the text of a date and time, the terminator included: 19 characters and the terminator. */
constexpr size_t TEXT_BYTES = 20;

/**
 * @brief Writes a date and time as `YYYY-MM-DD HH:MM:SS`, every number padded with zeros.
 *
 * @param time A valid date and time (`cock::abi::datetime::isValid`); a year above 9999 keeps its last four digits.
 * @param out Receives the text and its terminator.
 */
void formatDateTime(const cock::abi::datetime::DateTime &time, char (&out)[TEXT_BYTES]);

} // namespace cock::date

#endif // !DATE_FORMAT_HPP
