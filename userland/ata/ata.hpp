#ifndef USERLAND_ATA_ATA_HPP
#define USERLAND_ATA_ATA_HPP

#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>

namespace ata {

constexpr uint16_t COMMAND_BASE = 0x1F0;
constexpr uint32_t COMMAND_PORT_COUNT = 8;
constexpr uint16_t CONTROL_PORT = 0x3F6;
constexpr uint32_t CONTROL_PORT_COUNT = 1;

constexpr uint16_t DATA_PORT = COMMAND_BASE;
constexpr uint16_t SECTOR_COUNT_PORT = COMMAND_BASE + 2;
constexpr uint16_t LBA_LOW_PORT = COMMAND_BASE + 3;
constexpr uint16_t LBA_MID_PORT = COMMAND_BASE + 4;
constexpr uint16_t LBA_HIGH_PORT = COMMAND_BASE + 5;
constexpr uint16_t DRIVE_PORT = COMMAND_BASE + 6;
constexpr uint16_t COMMAND_PORT = COMMAND_BASE + 7;
constexpr uint16_t ALTERNATE_STATUS_PORT = CONTROL_PORT;

constexpr uint8_t STATUS_ERROR = 0x01;
constexpr uint8_t STATUS_DATA_REQUEST = 0x08;
constexpr uint8_t STATUS_FAULT = 0x20;
constexpr uint8_t STATUS_BUSY = 0x80;
constexpr uint8_t STATUS_NO_DEVICE = 0x00;
constexpr uint8_t STATUS_FLOATING_BUS = 0xFF;

constexpr uint8_t COMMAND_READ_SECTORS = 0x20;
constexpr uint8_t COMMAND_WRITE_SECTORS = 0x30;
constexpr uint8_t COMMAND_CACHE_FLUSH = 0xE7;
constexpr uint8_t COMMAND_IDENTIFY = 0xEC;

constexpr uint8_t CONTROL_INTERRUPTS_OFF = 0x02;
constexpr uint8_t DRIVE_IDENTIFY_MASTER = 0xA0;
constexpr uint8_t DRIVE_LBA_MASTER = 0xE0;
constexpr uint8_t LBA_LOW_MASK = 0xFF;
constexpr uint8_t LBA_HIGH_NIBBLE_MASK = 0x0F;
constexpr unsigned LBA_MID_SHIFT = 8;
constexpr unsigned LBA_HIGH_SHIFT = 16;
constexpr unsigned LBA_DRIVE_SHIFT = 24;

constexpr size_t SETTLE_READS = 4;
constexpr uint32_t POLL_LIMIT = 1000000;
constexpr size_t SECTOR_WORDS = 256;
constexpr size_t IDENTIFY_SECTORS_LOW_WORD = 60;
constexpr size_t IDENTIFY_SECTORS_HIGH_WORD = 61;
constexpr unsigned WORD_BITS = 16;
constexpr uint32_t MAX_LBA28_SECTORS = 0x0FFFFFFF;
constexpr uint32_t MAX_READ_SECTORS = 255;
constexpr uint32_t MAX_WRITE_SECTORS = 255;

class Drive {
  public:
	bool init() {
		if (!cock::user::request_ports(COMMAND_BASE, COMMAND_PORT_COUNT) ||
			!cock::user::request_ports(CONTROL_PORT, CONTROL_PORT_COUNT))
			return false;

		cock::user::port_out8(CONTROL_PORT, CONTROL_INTERRUPTS_OFF);
		return identify();
	}

	uint32_t sectors() const { return total; }

	bool read(uint32_t lba, uint32_t count, void *buffer) {
		if (count == 0 || count > MAX_READ_SECTORS || lba >= total ||
			count > total - lba)
			return false;

		uint8_t *cursor = static_cast<uint8_t *>(buffer);
		selectSectors(lba, count);
		cock::user::port_out8(COMMAND_PORT, COMMAND_READ_SECTORS);
		settle();

		for (uint32_t done = 0; done < count; done++) {
			if (!waitForData()) return false;
			cock::user::port_read_words(DATA_PORT, cursor, SECTOR_WORDS);
			cursor += SECTOR_WORDS * sizeof(uint16_t);
		}
		return true;
	}

	bool write(uint32_t lba, uint32_t count, const void *buffer) {
		if (count == 0 || count > MAX_WRITE_SECTORS || lba >= total ||
			count > total - lba)
			return false;

		const uint8_t *cursor = static_cast<const uint8_t *>(buffer);
		selectSectors(lba, count);
		cock::user::port_out8(COMMAND_PORT, COMMAND_WRITE_SECTORS);
		settle();

		for (uint32_t done = 0; done < count; done++) {
			if (!waitForData()) return false;
			cock::user::port_write_words(DATA_PORT, cursor, SECTOR_WORDS);
			cursor += SECTOR_WORDS * sizeof(uint16_t);
		}
		return waitForCompletion() && flush();
	}

  private:
	uint32_t total = 0;

	static void selectSectors(uint32_t lba, uint32_t count) {
		cock::user::port_out8(DRIVE_PORT, DRIVE_LBA_MASTER | ((lba >> LBA_DRIVE_SHIFT) &
															   LBA_HIGH_NIBBLE_MASK));
		cock::user::port_out8(SECTOR_COUNT_PORT, static_cast<uint8_t>(count));
		cock::user::port_out8(LBA_LOW_PORT, static_cast<uint8_t>(lba & LBA_LOW_MASK));
		cock::user::port_out8(
			LBA_MID_PORT, static_cast<uint8_t>((lba >> LBA_MID_SHIFT) & LBA_LOW_MASK));
		cock::user::port_out8(
			LBA_HIGH_PORT, static_cast<uint8_t>((lba >> LBA_HIGH_SHIFT) & LBA_LOW_MASK));
	}

	static bool waitForCompletion() {
		settle();
		if (!waitNotBusy()) return false;
		uint8_t status = cock::user::port_in8(COMMAND_PORT);
		return (status & (STATUS_ERROR | STATUS_FAULT)) == 0;
	}

	static bool flush() {
		cock::user::port_out8(COMMAND_PORT, COMMAND_CACHE_FLUSH);
		return waitForCompletion();
	}

	static void settle() {
		for (size_t read = 0; read < SETTLE_READS; read++)
			cock::user::port_in8(ALTERNATE_STATUS_PORT);
	}

	static bool waitNotBusy() {
		for (uint32_t spin = 0; spin < POLL_LIMIT; spin++)
			if ((cock::user::port_in8(ALTERNATE_STATUS_PORT) & STATUS_BUSY) == 0)
				return true;
		return false;
	}

	static bool waitForData() {
		settle();
		if (!waitNotBusy()) return false;
		uint8_t status = cock::user::port_in8(COMMAND_PORT);
		return (status & (STATUS_ERROR | STATUS_FAULT)) == 0 &&
			   (status & STATUS_DATA_REQUEST) != 0;
	}

	bool identify() {
		cock::user::port_out8(DRIVE_PORT, DRIVE_IDENTIFY_MASTER);
		settle();
		cock::user::port_out8(SECTOR_COUNT_PORT, 0);
		cock::user::port_out8(LBA_LOW_PORT, 0);
		cock::user::port_out8(LBA_MID_PORT, 0);
		cock::user::port_out8(LBA_HIGH_PORT, 0);
		cock::user::port_out8(COMMAND_PORT, COMMAND_IDENTIFY);
		settle();

		uint8_t status = cock::user::port_in8(COMMAND_PORT);
		if (status == STATUS_NO_DEVICE || status == STATUS_FLOATING_BUS) return false;
		if (!waitNotBusy()) return false;
		if (cock::user::port_in8(LBA_MID_PORT) != 0 ||
			cock::user::port_in8(LBA_HIGH_PORT) != 0)
			return false;
		if (!waitForData()) return false;

		uint16_t words[SECTOR_WORDS];
		cock::user::port_read_words(DATA_PORT, words, SECTOR_WORDS);
		uint32_t sectors = static_cast<uint32_t>(words[IDENTIFY_SECTORS_LOW_WORD]) |
						   (static_cast<uint32_t>(words[IDENTIFY_SECTORS_HIGH_WORD])
							<< WORD_BITS);
		if (sectors == 0) return false;
		total = sectors > MAX_LBA28_SECTORS ? MAX_LBA28_SECTORS : sectors;
		return true;
	}
};

} // namespace ata

#endif // !USERLAND_ATA_ATA_HPP
