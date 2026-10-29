/**
 * @file cmos.hpp
 * @brief Direct reader of the CMOS real time clock through I/O ports 0x70 and 0x71.
 *
 * Meant for the RTC server only, which holds the `IO_PORTS` capability for the pair of
 * ports. Other programs use `Clock` (see `clock.hpp`). All calls are synchronous port
 * accesses and never use system calls except `request_ports` in `init`.
 */
#ifndef COCK_USER_CMOS_HPP
#define COCK_USER_CMOS_HPP

#include <cock/abi/datetime.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

/** CMOS clock reader: waits for a stable moment, reads twice and decodes the registers. */
class Cmos {
  public:
	/** Port that selects the CMOS register. */
	static constexpr uint16_t INDEX_PORT = 0x70;
	/** Port that reads or writes the selected register. */
	static constexpr uint16_t DATA_PORT = 0x71;
	/** Number of consecutive ports, starting at `INDEX_PORT`. */
	static constexpr uint32_t PORT_COUNT = 2;

	/**
	 * @brief Asks the kernel for the CMOS ports (`SYS_REQUEST_RESOURCE`).
	 * @return true when granted; false when the process has no `IO_PORTS` capability
	 *         for them. `now` fails until this succeeds.
	 */
	bool init() {
		granted = request_ports(INDEX_PORT, PORT_COUNT);
		return granted;
	}

	/**
	 * @brief Reads the current date and time.
	 *
	 * Waits (spinning, bounded) until the clock is not updating, reads all registers and
	 * accepts the value once two consecutive reads agree, so a read torn by an update is
	 * never returned. Handles BCD or binary, 12 or 24 hour modes and the century register.
	 * @param time Receives the date and time on success.
	 * @return true on success; false when `init` did not succeed, the clock stayed busy,
	 *         no two reads agreed within the attempt limit, or the decoded value is not a
	 *         valid date.
	 */
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
	/** Register index of the seconds. */
	static constexpr uint8_t SECONDS = 0x00;
	/** Register index of the minutes. */
	static constexpr uint8_t MINUTES = 0x02;
	/** Register index of the hours. */
	static constexpr uint8_t HOURS = 0x04;
	/** Register index of the day of the month. */
	static constexpr uint8_t DAY = 0x07;
	/** Register index of the month. */
	static constexpr uint8_t MONTH = 0x08;
	/** Register index of the year within the century. */
	static constexpr uint8_t YEAR = 0x09;
	/** Register index of the century (not present on every machine). */
	static constexpr uint8_t CENTURY = 0x32;
	/** Index of status register A. */
	static constexpr uint8_t STATUS_A = 0x0A;
	/** Index of status register B. */
	static constexpr uint8_t STATUS_B = 0x0B;
	/** Bit of status A that is set while the clock updates its registers. */
	static constexpr uint8_t UPDATE_IN_PROGRESS = 0x80;
	/** Bit of status B that is set when values are binary instead of BCD. */
	static constexpr uint8_t BINARY_MODE = 0x04;
	/** Bit of status B that is set when the hours use the 24 hour format. */
	static constexpr uint8_t TWENTY_FOUR_HOUR_MODE = 0x02;
	/** Bit of the hours register that marks PM in 12 hour mode. */
	static constexpr uint8_t PM_FLAG = 0x80;
	/** Mask that removes the PM flag from the hours register. */
	static constexpr uint8_t HOUR_MASK = 0x7F;
	/** Most polls of status A while waiting for the update to end. */
	static constexpr uint32_t SPIN_LIMIT = 1000000;
	/** Most read rounds before giving up on getting two equal reads. */
	static constexpr uint32_t MAX_ATTEMPTS = 8;
	/** Hours on a 12 hour clock. */
	static constexpr uint32_t TWELVE = 12;
	/** Century used when the century register holds an implausible value. */
	static constexpr uint32_t DEFAULT_CENTURY = 20;
	/** Smallest century accepted from the register. */
	static constexpr uint32_t FIRST_CENTURY = 19;
	/** Largest century accepted from the register. */
	static constexpr uint32_t LAST_CENTURY = 21;
	/** Years in a century. */
	static constexpr uint32_t CENTURY_YEARS = 100;

	/** Undecoded copy of the clock registers. */
	struct Raw {
		/** Seconds register. */
		uint8_t seconds;
		/** Minutes register. */
		uint8_t minutes;
		/** Hours register, may carry the PM flag. */
		uint8_t hours;
		/** Day register. */
		uint8_t day;
		/** Month register. */
		uint8_t month;
		/** Year register (within the century). */
		uint8_t year;
		/** Century register. */
		uint8_t century;
		/** Status register B, which says how to decode the others. */
		uint8_t statusB;
	};

	/** true after a successful `init`. */
	bool granted = false;

	/**
	 * @brief Reads one CMOS register through the index and data ports.
	 * @param index Register index.
	 * @return The register value.
	 */
	static uint8_t readRegister(uint8_t index) {
		port_out8(INDEX_PORT, index);
		return port_in8(DATA_PORT);
	}

	/**
	 * @brief Spins until the clock is not in the middle of an update.
	 * @return true when the clock is stable; false after `SPIN_LIMIT` polls.
	 */
	static bool waitForStableTime() {
		for (uint32_t spin = 0; spin < SPIN_LIMIT; spin++)
			if ((readRegister(STATUS_A) & UPDATE_IN_PROGRESS) == 0) return true;
		return false;
	}

	/**
	 * @brief Reads every clock register once.
	 * @return The raw values.
	 */
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

	/**
	 * @brief Compares two raw reads field by field.
	 * @param first First read.
	 * @param second Second read.
	 * @return true when every field is equal.
	 */
	static bool same(const Raw &first, const Raw &second) {
		return first.seconds == second.seconds && first.minutes == second.minutes &&
			   first.hours == second.hours && first.day == second.day &&
			   first.month == second.month && first.year == second.year &&
			   first.century == second.century && first.statusB == second.statusB;
	}

	/**
	 * @brief Decodes one register according to the BCD or binary mode in status B.
	 * @param raw Read that holds status B.
	 * @param value Register value to decode.
	 * @return The binary value.
	 */
	static uint32_t decode(const Raw &raw, uint8_t value) {
		return (raw.statusB & BINARY_MODE) != 0 ? value : abi::datetime::bcdToBinary(value);
	}

	/**
	 * @brief Turns a raw read into a date and time.
	 * @param raw Stable read.
	 * @param time Receives the decoded value.
	 * @return true when the result is a valid date and time.
	 */
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

#endif // !COCK_USER_CMOS_HPP
