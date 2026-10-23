/**
 * @file datetime.hpp
 * @brief Calendar date and time (UTC, Gregorian) with conversions to Unix time, FAT and BCD.
 *
 * Pure functions with no dependencies, used by the clock server and the file system servers.
 */
#ifndef SHARED_COCK_ABI_DATETIME_HPP
#define SHARED_COCK_ABI_DATETIME_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::datetime {

/** Seconds in a minute. */
constexpr uint32_t SECONDS_PER_MINUTE = 60;
/** Minutes in an hour. */
constexpr uint32_t MINUTES_PER_HOUR = 60;
/** Hours in a day. */
constexpr uint32_t HOURS_PER_DAY = 24;
/** Seconds in an hour. */
constexpr uint32_t SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
/** Seconds in a day. */
constexpr uint32_t SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY;
/** Months in a year. */
constexpr uint32_t MONTHS_PER_YEAR = 12;
/** Epoch year of Unix time; also the lowest year `isValid` accepts. */
constexpr uint32_t UNIX_FIRST_YEAR = 1970;
/** First year a FAT date can hold. */
constexpr uint32_t FAT_FIRST_YEAR = 1980;
/** Last year a FAT date can hold (7 bit year field). */
constexpr uint32_t FAT_LAST_YEAR = 2107;
/** Years between leap years. */
constexpr uint32_t LEAP_CYCLE = 4;
/** Century years are not leap years... */
constexpr uint32_t CENTURY_CYCLE = 100;
/** ...unless divisible by 400. */
constexpr uint32_t QUAD_CENTURY_CYCLE = 400;
/** Days in a common year. */
constexpr uint32_t DAYS_PER_YEAR = 365;
/** Value of one BCD tens digit. */
constexpr uint32_t BCD_RADIX = 10;
/** Bit position of the tens digit of a BCD byte. */
constexpr unsigned BCD_SHIFT = 4;
/** Mask of the units digit of a BCD byte. */
constexpr uint8_t BCD_LOW_MASK = 0x0F;
/** Bit position of the year (since 1980) in a FAT date. */
constexpr unsigned FAT_YEAR_SHIFT = 9;
/** Bit position of the month in a FAT date. */
constexpr unsigned FAT_MONTH_SHIFT = 5;
/** Bit position of the hour in a FAT time. */
constexpr unsigned FAT_HOUR_SHIFT = 11;
/** Bit position of the minute in a FAT time. */
constexpr unsigned FAT_MINUTE_SHIFT = 5;
/** FAT times have a resolution of two seconds. */
constexpr uint32_t FAT_SECOND_STEP = 2;
/** Days of each month in a common year (index 0 is January). */
constexpr uint32_t DAYS_IN_MONTH[MONTHS_PER_YEAR] = {31, 28, 31, 30, 31, 30,
													 31, 31, 30, 31, 30, 31};
/** Number of the month that gains a day in leap years. */
constexpr uint32_t FEBRUARY = 2;

/** Broken down date and time. Valid ranges: see `isValid`. */
struct DateTime {
	/** Full year, 1970 or later. */
	uint32_t year;
	/** Month, 1 to 12. */
	uint32_t month;
	/** Day of the month, 1 to the length of the month. */
	uint32_t day;
	/** Hour, 0 to 23. */
	uint32_t hour;
	/** Minute, 0 to 59. */
	uint32_t minute;
	/** Second, 0 to 59. */
	uint32_t second;
};

/**
 * @brief Converts one binary coded decimal byte (as read from the CMOS clock).
 * @param value Byte with tens in the high nibble and units in the low nibble.
 * @return The binary value. Nibbles above 9 are not rejected.
 */
inline uint32_t bcdToBinary(uint8_t value) {
	return uint32_t(value >> BCD_SHIFT) * BCD_RADIX + (value & BCD_LOW_MASK);
}

/**
 * @brief Gregorian leap year rule.
 * @param year Full year.
 * @return True for years divisible by 4 except centuries not divisible by 400.
 */
inline bool isLeapYear(uint32_t year) {
	return (year % LEAP_CYCLE == 0 && year % CENTURY_CYCLE != 0) ||
		   year % QUAD_CENTURY_CYCLE == 0;
}

/**
 * @brief Length of a month.
 * @param year Full year (decides February).
 * @param month Month, 1 to 12; any other value reads outside the table, so validate first.
 * @return Number of days, 28 to 31.
 */
inline uint32_t daysInMonth(uint32_t year, uint32_t month) {
	if (month == FEBRUARY && isLeapYear(year)) return DAYS_IN_MONTH[month - 1] + 1;
	return DAYS_IN_MONTH[month - 1];
}

/**
 * @brief Checks that every field is in range.
 * @param time Date and time to check.
 * @return True when the year is 1970 or later, the month is 1 to 12, the day exists in that month, the hour is below 24 and the minute and second are below 60.
 */
inline bool isValid(const DateTime &time) {
	return time.year >= UNIX_FIRST_YEAR && time.month >= 1 && time.month <= MONTHS_PER_YEAR &&
		   time.day >= 1 && time.day <= daysInMonth(time.year, time.month) &&
		   time.hour < HOURS_PER_DAY && time.minute < MINUTES_PER_HOUR &&
		   time.second < SECONDS_PER_MINUTE;
}

/**
 * @brief Converts a date and time to seconds since 1970-01-01 00:00:00 UTC.
 * @param time A valid date and time (see `isValid`); not validated here.
 * @return Seconds since the epoch; wraps after the year 2106 (32 bit).
 */
inline uint32_t toUnix(const DateTime &time) {
	uint32_t days = 0;
	for (uint32_t year = UNIX_FIRST_YEAR; year < time.year; year++)
		days += DAYS_PER_YEAR + (isLeapYear(year) ? 1 : 0);
	for (uint32_t month = 1; month < time.month; month++)
		days += daysInMonth(time.year, month);
	days += time.day - 1;
	return days * SECONDS_PER_DAY + time.hour * SECONDS_PER_HOUR +
		   time.minute * SECONDS_PER_MINUTE + time.second;
}

/**
 * @brief Encodes the date as a FAT directory entry date.
 * @param time Date; years outside 1980 to 2107 are clamped.
 * @return Bits 9 to 15 year since 1980, bits 5 to 8 month, bits 0 to 4 day.
 */
inline uint16_t toFatDate(const DateTime &time) {
	uint32_t year = time.year < FAT_FIRST_YEAR ? FAT_FIRST_YEAR
					: time.year > FAT_LAST_YEAR ? FAT_LAST_YEAR
												: time.year;
	return uint16_t(((year - FAT_FIRST_YEAR) << FAT_YEAR_SHIFT) |
					(time.month << FAT_MONTH_SHIFT) | time.day);
}

/**
 * @brief Encodes the time as a FAT directory entry time.
 * @param time Time; odd seconds are rounded down to even.
 * @return Bits 11 to 15 hour, bits 5 to 10 minute, bits 0 to 4 seconds divided by two.
 */
inline uint16_t toFatTime(const DateTime &time) {
	return uint16_t((time.hour << FAT_HOUR_SHIFT) | (time.minute << FAT_MINUTE_SHIFT) |
					(time.second / FAT_SECOND_STEP));
}

} // namespace cock::abi::datetime

#endif // !SHARED_COCK_ABI_DATETIME_HPP
