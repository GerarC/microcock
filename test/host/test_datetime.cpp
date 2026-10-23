#include <cock/abi/datetime.hpp>
#include <cock/abi/rtc.hpp>
#include <test/host/check.hpp>
#include <stdint.h>

namespace host_test {

namespace {

using namespace cock::abi::datetime;

constexpr DateTime SAMPLE = {2026, 10, 15, 13, 45, 30};
constexpr DateTime UNIX_START = {1970, 1, 1, 0, 0, 0};
constexpr DateTime YEAR_2000 = {2000, 1, 1, 0, 0, 0};
constexpr DateTime LEAP_DAY = {2024, 2, 29, 12, 34, 56};
constexpr DateTime FAR_FUTURE = {2200, 12, 31, 0, 0, 0};
constexpr uint32_t LEAP_DAY_UNIX = 1709210096;
constexpr uint32_t YEAR_2000_UNIX = 946684800;
constexpr uint16_t FAT_DATE_SAMPLE = 23887;
constexpr uint16_t FAT_TIME_SAMPLE = 28079;
constexpr uint8_t BCD_FIFTY_NINE = 0x59;
constexpr uint32_t FIFTY_NINE = 59;
constexpr uint8_t BCD_TWENTY_SIX = 0x26;
constexpr uint32_t TWENTY_SIX = 26;
constexpr uint32_t LEAP_YEAR_DIVISIBLE_BY_400 = 2000;
constexpr uint32_t NOT_LEAP_DIVISIBLE_BY_100 = 1900;
constexpr uint32_t LEAP_YEAR = 2024;
constexpr uint32_t COMMON_YEAR = 2023;
constexpr uint32_t DAYS_FEBRUARY_LEAP = 29;
constexpr uint32_t DAYS_FEBRUARY_COMMON = 28;
constexpr uint32_t DAYS_DECEMBER = 31;
constexpr uint32_t DAYS_APRIL = 30;
constexpr uint32_t APRIL = 4;
constexpr uint32_t DECEMBER = 12;
constexpr uint32_t MONTH_PAST_END = 13;
constexpr uint32_t DAY_PAST_APRIL = 31;
constexpr uint32_t HOUR_PAST_END = 24;
constexpr uint32_t MINUTE_PAST_END = 60;
constexpr uint32_t YEAR_BEFORE_UNIX = 1969;
constexpr uint32_t NO_VALUE = 0;
constexpr uint32_t FIRST_VALUE = 1;

void checkCalendar() {
	CHECK_EQ(FIFTY_NINE, bcdToBinary(BCD_FIFTY_NINE));
	CHECK_EQ(TWENTY_SIX, bcdToBinary(BCD_TWENTY_SIX));
	CHECK_EQ(NO_VALUE, bcdToBinary(0));

	CHECK_TRUE(isLeapYear(LEAP_YEAR_DIVISIBLE_BY_400));
	CHECK_FALSE(isLeapYear(NOT_LEAP_DIVISIBLE_BY_100));
	CHECK_TRUE(isLeapYear(LEAP_YEAR));
	CHECK_FALSE(isLeapYear(COMMON_YEAR));
	CHECK_EQ(DAYS_FEBRUARY_LEAP, daysInMonth(LEAP_YEAR, FEBRUARY));
	CHECK_EQ(DAYS_FEBRUARY_COMMON, daysInMonth(COMMON_YEAR, FEBRUARY));
	CHECK_EQ(DAYS_DECEMBER, daysInMonth(COMMON_YEAR, DECEMBER));
	CHECK_EQ(DAYS_APRIL, daysInMonth(COMMON_YEAR, APRIL));

	CHECK_TRUE(isValid(SAMPLE));

	DateTime broken = SAMPLE;
	broken.month = MONTH_PAST_END;
	CHECK_FALSE(isValid(broken));
	broken = SAMPLE;
	broken.month = NO_VALUE;
	CHECK_FALSE(isValid(broken));
	broken = SAMPLE;
	broken.month = APRIL;
	broken.day = DAY_PAST_APRIL;
	CHECK_FALSE(isValid(broken));
	broken = SAMPLE;
	broken.year = COMMON_YEAR;
	broken.month = FEBRUARY;
	broken.day = DAYS_FEBRUARY_LEAP;
	CHECK_FALSE(isValid(broken));
	broken = SAMPLE;
	broken.hour = HOUR_PAST_END;
	CHECK_FALSE(isValid(broken));
	broken = SAMPLE;
	broken.minute = MINUTE_PAST_END;
	CHECK_FALSE(isValid(broken));
	broken = SAMPLE;
	broken.year = YEAR_BEFORE_UNIX;
	CHECK_FALSE(isValid(broken));
}

void checkConversions() {
	CHECK_EQ(NO_VALUE, toUnix(UNIX_START));
	CHECK_EQ(YEAR_2000_UNIX, toUnix(YEAR_2000));
	CHECK_EQ(LEAP_DAY_UNIX, toUnix(LEAP_DAY));

	CHECK_EQ(FAT_DATE_SAMPLE, toFatDate(SAMPLE));
	CHECK_EQ(FAT_TIME_SAMPLE, toFatTime(SAMPLE));
	CHECK_EQ(uint16_t((FIRST_VALUE << FAT_MONTH_SHIFT) | FIRST_VALUE), toFatDate(UNIX_START));
	CHECK_EQ(uint16_t(((FAT_LAST_YEAR - FAT_FIRST_YEAR) << FAT_YEAR_SHIFT) |
					  (DECEMBER << FAT_MONTH_SHIFT) | DAYS_DECEMBER),
			 toFatDate(FAR_FUTURE));
}

void checkPacking() {
	DateTime back = {};
	CHECK_TRUE(cock::abi::rtc::unpack(cock::abi::rtc::packDate(SAMPLE),
									  cock::abi::rtc::packTime(SAMPLE), back));
	CHECK_EQ(SAMPLE.year, back.year);
	CHECK_EQ(SAMPLE.month, back.month);
	CHECK_EQ(SAMPLE.day, back.day);
	CHECK_EQ(SAMPLE.hour, back.hour);
	CHECK_EQ(SAMPLE.minute, back.minute);
	CHECK_EQ(SAMPLE.second, back.second);

	CHECK_FALSE(cock::abi::rtc::unpack(NO_VALUE, NO_VALUE, back));
	CHECK_TRUE(cock::abi::rtc::unpack(cock::abi::rtc::packDate(LEAP_DAY),
									  cock::abi::rtc::packTime(LEAP_DAY), back));
	CHECK_EQ(LEAP_DAY.second, back.second);
}

} // namespace

void testDatetime() {
	checkCalendar();
	checkConversions();
	checkPacking();
}

} // namespace host_test
