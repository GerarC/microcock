#ifndef COCK_USER_CLOCK_HPP
#define COCK_USER_CLOCK_HPP

#include <cock/abi/datetime.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

class Clock {
  public:
	static constexpr uint16_t INDEX_PORT = 0x70;
	static constexpr uint16_t DATA_PORT = 0x71;
	static constexpr uint32_t PORT_COUNT = 2;

	bool init() {
		granted = request_ports(INDEX_PORT, PORT_COUNT);
		return granted;
	}

	bool now(abi::datetime::DateTime &time) {
		if (!granted) return false;

		Raw previous = {};
		bool havePrevious = false;
		for (uint32_t attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
			if (!waitForStableTime()) return false;
			Raw current = readRaw();
			if (havePrevious && same(previous, current)) return convert(current, time);
			previous = current;
			havePrevious = true;
		}
		return false;
	}

  private:
	static constexpr uint8_t SECONDS = 0x00;
	static constexpr uint8_t MINUTES = 0x02;
	static constexpr uint8_t HOURS = 0x04;
	static constexpr uint8_t DAY = 0x07;
	static constexpr uint8_t MONTH = 0x08;
	static constexpr uint8_t YEAR = 0x09;
	static constexpr uint8_t CENTURY = 0x32;
	static constexpr uint8_t STATUS_A = 0x0A;
	static constexpr uint8_t STATUS_B = 0x0B;
	static constexpr uint8_t UPDATE_IN_PROGRESS = 0x80;
	static constexpr uint8_t BINARY_MODE = 0x04;
	static constexpr uint8_t TWENTY_FOUR_HOUR_MODE = 0x02;
	static constexpr uint8_t PM_FLAG = 0x80;
	static constexpr uint8_t HOUR_MASK = 0x7F;
	static constexpr uint32_t SPIN_LIMIT = 1000000;
	static constexpr uint32_t MAX_ATTEMPTS = 8;
	static constexpr uint32_t TWELVE = 12;
	static constexpr uint32_t DEFAULT_CENTURY = 20;
	static constexpr uint32_t FIRST_CENTURY = 19;
	static constexpr uint32_t LAST_CENTURY = 21;
	static constexpr uint32_t CENTURY_YEARS = 100;

	struct Raw {
		uint8_t seconds;
		uint8_t minutes;
		uint8_t hours;
		uint8_t day;
		uint8_t month;
		uint8_t year;
		uint8_t century;
		uint8_t statusB;
	};

	bool granted = false;

	static uint8_t readRegister(uint8_t index) {
		port_out8(INDEX_PORT, index);
		return port_in8(DATA_PORT);
	}

	static bool waitForStableTime() {
		for (uint32_t spin = 0; spin < SPIN_LIMIT; spin++)
			if ((readRegister(STATUS_A) & UPDATE_IN_PROGRESS) == 0) return true;
		return false;
	}

	static Raw readRaw() {
		Raw raw = {};
		raw.seconds = readRegister(SECONDS);
		raw.minutes = readRegister(MINUTES);
		raw.hours = readRegister(HOURS);
		raw.day = readRegister(DAY);
		raw.month = readRegister(MONTH);
		raw.year = readRegister(YEAR);
		raw.century = readRegister(CENTURY);
		raw.statusB = readRegister(STATUS_B);
		return raw;
	}

	static bool same(const Raw &first, const Raw &second) {
		return first.seconds == second.seconds && first.minutes == second.minutes &&
			   first.hours == second.hours && first.day == second.day &&
			   first.month == second.month && first.year == second.year &&
			   first.century == second.century && first.statusB == second.statusB;
	}

	static uint32_t decode(const Raw &raw, uint8_t value) {
		return (raw.statusB & BINARY_MODE) != 0 ? value : abi::datetime::bcdToBinary(value);
	}

	static bool convert(const Raw &raw, abi::datetime::DateTime &time) {
		bool pm = (raw.statusB & TWENTY_FOUR_HOUR_MODE) == 0 && (raw.hours & PM_FLAG) != 0;
		uint32_t hour = decode(raw, raw.hours & HOUR_MASK);
		if ((raw.statusB & TWENTY_FOUR_HOUR_MODE) == 0) hour = hour % TWELVE + (pm ? TWELVE : 0);

		uint32_t century = decode(raw, raw.century);
		if (century < FIRST_CENTURY || century > LAST_CENTURY) century = DEFAULT_CENTURY;

		time.year = century * CENTURY_YEARS + decode(raw, raw.year);
		time.month = decode(raw, raw.month);
		time.day = decode(raw, raw.day);
		time.hour = hour;
		time.minute = decode(raw, raw.minutes);
		time.second = decode(raw, raw.seconds);
		return abi::datetime::isValid(time);
	}
};

} // namespace cock::user

#endif // !COCK_USER_CLOCK_HPP
