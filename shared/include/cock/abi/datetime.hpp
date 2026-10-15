#ifndef SHARED_COCK_ABI_DATETIME_HPP
#define SHARED_COCK_ABI_DATETIME_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::datetime {

constexpr uint32_t SECONDS_PER_MINUTE = 60;
constexpr uint32_t MINUTES_PER_HOUR = 60;
constexpr uint32_t HOURS_PER_DAY = 24;
constexpr uint32_t SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
constexpr uint32_t SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY;
constexpr uint32_t MONTHS_PER_YEAR = 12;
constexpr uint32_t UNIX_FIRST_YEAR = 1970;
constexpr uint32_t FAT_FIRST_YEAR = 1980;
constexpr uint32_t FAT_LAST_YEAR = 2107;
constexpr uint32_t LEAP_CYCLE = 4;
constexpr uint32_t CENTURY_CYCLE = 100;
constexpr uint32_t QUAD_CENTURY_CYCLE = 400;
constexpr uint32_t DAYS_PER_YEAR = 365;
constexpr uint32_t BCD_RADIX = 10;
constexpr unsigned BCD_SHIFT = 4;
constexpr uint8_t BCD_LOW_MASK = 0x0F;
constexpr unsigned FAT_YEAR_SHIFT = 9;
constexpr unsigned FAT_MONTH_SHIFT = 5;
constexpr unsigned FAT_HOUR_SHIFT = 11;
constexpr unsigned FAT_MINUTE_SHIFT = 5;
constexpr uint32_t FAT_SECOND_STEP = 2;
constexpr uint32_t DAYS_IN_MONTH[MONTHS_PER_YEAR] = {31, 28, 31, 30, 31, 30,
													 31, 31, 30, 31, 30, 31};
constexpr uint32_t FEBRUARY = 2;

struct DateTime {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
};

inline uint32_t bcdToBinary(uint8_t value) {
	return uint32_t(value >> BCD_SHIFT) * BCD_RADIX + (value & BCD_LOW_MASK);
}

inline bool isLeapYear(uint32_t year) {
	return (year % LEAP_CYCLE == 0 && year % CENTURY_CYCLE != 0) ||
		   year % QUAD_CENTURY_CYCLE == 0;
}

inline uint32_t daysInMonth(uint32_t year, uint32_t month) {
	if (month == FEBRUARY && isLeapYear(year)) return DAYS_IN_MONTH[month - 1] + 1;
	return DAYS_IN_MONTH[month - 1];
}

inline bool isValid(const DateTime &time) {
	return time.year >= UNIX_FIRST_YEAR && time.month >= 1 && time.month <= MONTHS_PER_YEAR &&
		   time.day >= 1 && time.day <= daysInMonth(time.year, time.month) &&
		   time.hour < HOURS_PER_DAY && time.minute < MINUTES_PER_HOUR &&
		   time.second < SECONDS_PER_MINUTE;
}

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

inline uint16_t toFatDate(const DateTime &time) {
	uint32_t year = time.year < FAT_FIRST_YEAR ? FAT_FIRST_YEAR
					: time.year > FAT_LAST_YEAR ? FAT_LAST_YEAR
												: time.year;
	return uint16_t(((year - FAT_FIRST_YEAR) << FAT_YEAR_SHIFT) |
					(time.month << FAT_MONTH_SHIFT) | time.day);
}

inline uint16_t toFatTime(const DateTime &time) {
	return uint16_t((time.hour << FAT_HOUR_SHIFT) | (time.minute << FAT_MINUTE_SHIFT) |
					(time.second / FAT_SECOND_STEP));
}

} // namespace cock::abi::datetime

#endif // !SHARED_COCK_ABI_DATETIME_HPP
