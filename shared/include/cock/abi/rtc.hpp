/**
 * @file rtc.hpp
 * @brief Wire protocol of the clock server (`rtc`).
 *
 * A client sends `RTC_READ` (`arg0` request id) to the process named `SERVER_NAME`. The server answers with
 * `RTC_REPLY`: `arg0` request id, `arg1` a `Status`, `arg2` the packed date and `arg3` the packed time.
 */
#ifndef COCK_ABI_RTC_HPP
#define COCK_ABI_RTC_HPP

#include <cock/abi/datetime.hpp>
#include <stdint.h>

namespace cock::abi::rtc {

/** Name of the clock server for the name server. */
constexpr char SERVER_NAME[] = "rtc";

/** Result code in `arg1` of `RTC_REPLY`. */
enum class Status : uint32_t {
	/** The date and time words are valid. */
	OK = 0,
	/** The clock could not be read or returned an invalid date. */
	UNAVAILABLE = 1,
};

/** Shift of the middle field of a packed word. */
constexpr uint32_t BYTE_BITS = 8;
/** Shift of the top field of a packed word. */
constexpr uint32_t PAIR_BITS = 16;
/** Mask of an 8 bit field. */
constexpr uint32_t BYTE_MASK = 0xFF;
/** Mask of the 16 bit year field. */
constexpr uint32_t PAIR_MASK = 0xFFFF;

/**
 * @brief Packs the date into one word.
 * @param time Date and time; only year, month and day are used.
 * @return year << 16 | month << 8 | day.
 */
inline uint32_t packDate(const datetime::DateTime &time) {
	return (time.year << PAIR_BITS) | (time.month << BYTE_BITS) | time.day;
}

/**
 * @brief Packs the time into one word.
 * @param time Date and time; only hour, minute and second are used.
 * @return hour << 16 | minute << 8 | second.
 */
inline uint32_t packTime(const datetime::DateTime &time) {
	return (time.hour << PAIR_BITS) | (time.minute << BYTE_BITS) | time.second;
}

/**
 * @brief Rebuilds a date and time from the two packed words of a reply.
 * @param date Packed date (see `packDate`).
 * @param time Packed time (see `packTime`).
 * @param out Receives the fields.
 * @return True when the result passes `datetime::isValid`; the client must not trust `out` otherwise.
 */
inline bool unpack(uint32_t date, uint32_t time, datetime::DateTime &out) {
	out.year = (date >> PAIR_BITS) & PAIR_MASK;
	out.month = (date >> BYTE_BITS) & BYTE_MASK;
	out.day = date & BYTE_MASK;
	out.hour = (time >> PAIR_BITS) & BYTE_MASK;
	out.minute = (time >> BYTE_BITS) & BYTE_MASK;
	out.second = time & BYTE_MASK;
	return datetime::isValid(out);
}

} // namespace cock::abi::rtc

#endif // !COCK_ABI_RTC_HPP
