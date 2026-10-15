#ifndef COCK_ABI_RTC_HPP
#define COCK_ABI_RTC_HPP

#include <cock/abi/datetime.hpp>
#include <stdint.h>

namespace cock::abi::rtc {

constexpr char SERVER_NAME[] = "rtc";

enum class Status : uint32_t {
	OK = 0,
	UNAVAILABLE = 1,
};

constexpr uint32_t BYTE_BITS = 8;
constexpr uint32_t PAIR_BITS = 16;
constexpr uint32_t BYTE_MASK = 0xFF;
constexpr uint32_t PAIR_MASK = 0xFFFF;

inline uint32_t packDate(const datetime::DateTime &time) {
	return (time.year << PAIR_BITS) | (time.month << BYTE_BITS) | time.day;
}

inline uint32_t packTime(const datetime::DateTime &time) {
	return (time.hour << PAIR_BITS) | (time.minute << BYTE_BITS) | time.second;
}

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
