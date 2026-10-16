/**
 * @file ata.hpp
 * @brief Polling ATA PIO driver for the primary channel, master drive, LBA28.
 *
 * Runs in the ATA driver process in Ring 3 and talks to the controller with port I/O
 * only (ports 0x1F0 to 0x1F7 and 0x3F6, granted by `request_ports`). Interrupts are
 * disabled and every wait is a bounded spin on the status register, so each call blocks
 * the process (busy waits) until the drive finishes or `POLL_LIMIT` is reached.
 */
#ifndef USERLAND_ATA_ATA_HPP
#define USERLAND_ATA_ATA_HPP

#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>

namespace ata {

/** First port of the command block of the primary channel. */
constexpr uint16_t COMMAND_BASE = 0x1F0;
/** Number of ports in the command block. */
constexpr uint32_t COMMAND_PORT_COUNT = 8;
/** Control block port (alternate status on read, device control on write). */
constexpr uint16_t CONTROL_PORT = 0x3F6;
/** Number of ports requested for the control block. */
constexpr uint32_t CONTROL_PORT_COUNT = 1;

/** Data register (16 bit transfers). */
constexpr uint16_t DATA_PORT = COMMAND_BASE;
/** Sector count register. */
constexpr uint16_t SECTOR_COUNT_PORT = COMMAND_BASE + 2;
/** LBA bits 0 to 7. */
constexpr uint16_t LBA_LOW_PORT = COMMAND_BASE + 3;
/** LBA bits 8 to 15. */
constexpr uint16_t LBA_MID_PORT = COMMAND_BASE + 4;
/** LBA bits 16 to 23. */
constexpr uint16_t LBA_HIGH_PORT = COMMAND_BASE + 5;
/** Drive and head select register (also LBA bits 24 to 27). */
constexpr uint16_t DRIVE_PORT = COMMAND_BASE + 6;
/** Command register on write, status register on read. */
constexpr uint16_t COMMAND_PORT = COMMAND_BASE + 7;
/** Alternate status register; reading it does not clear a pending interrupt. */
constexpr uint16_t ALTERNATE_STATUS_PORT = CONTROL_PORT;

/** Status bit: the last command ended with an error. */
constexpr uint8_t STATUS_ERROR = 0x01;
/** Status bit: the drive is ready to transfer data. */
constexpr uint8_t STATUS_DATA_REQUEST = 0x08;
/** Status bit: the drive reported a fault. */
constexpr uint8_t STATUS_FAULT = 0x20;
/** Status bit: the drive is busy and other bits are not valid. */
constexpr uint8_t STATUS_BUSY = 0x80;
/** Status value read when no drive answers. */
constexpr uint8_t STATUS_NO_DEVICE = 0x00;
/** Status value read from a floating bus (no controller). */
constexpr uint8_t STATUS_FLOATING_BUS = 0xFF;

/** Command: read sectors with PIO. */
constexpr uint8_t COMMAND_READ_SECTORS = 0x20;
/** Command: write sectors with PIO. */
constexpr uint8_t COMMAND_WRITE_SECTORS = 0x30;
/** Command: flush the write cache of the drive. */
constexpr uint8_t COMMAND_CACHE_FLUSH = 0xE7;
/** Command: read the identification block of the drive. */
constexpr uint8_t COMMAND_IDENTIFY = 0xEC;

/** Device control value that disables the interrupt of the drive. */
constexpr uint8_t CONTROL_INTERRUPTS_OFF = 0x02;
/** Drive select value of the master for IDENTIFY. */
constexpr uint8_t DRIVE_IDENTIFY_MASTER = 0xA0;
/** Drive select value of the master with LBA addressing. */
constexpr uint8_t DRIVE_LBA_MASTER = 0xE0;
/** Mask of one byte of the LBA. */
constexpr uint8_t LBA_LOW_MASK = 0xFF;
/** Mask of the LBA bits 24 to 27 kept in the drive register. */
constexpr uint8_t LBA_HIGH_NIBBLE_MASK = 0x0F;
/** Shift that brings LBA bits 8 to 15 to the low byte. */
constexpr unsigned LBA_MID_SHIFT = 8;
/** Shift that brings LBA bits 16 to 23 to the low byte. */
constexpr unsigned LBA_HIGH_SHIFT = 16;
/** Shift that brings LBA bits 24 to 27 to the low nibble. */
constexpr unsigned LBA_DRIVE_SHIFT = 24;

/** Dummy status reads that give the drive 400 ns to update its status after a command. */
constexpr size_t SETTLE_READS = 4;
/** Most polls of the status register before a wait gives up. */
constexpr uint32_t POLL_LIMIT = 1000000;
/** Number of 16 bit words in a sector (512 bytes). */
constexpr size_t SECTOR_WORDS = 256;
/** Index of the low word of the LBA28 sector count in the IDENTIFY data. */
constexpr size_t IDENTIFY_SECTORS_LOW_WORD = 60;
/** Index of the high word of the LBA28 sector count in the IDENTIFY data. */
constexpr size_t IDENTIFY_SECTORS_HIGH_WORD = 61;
/** Bits in an IDENTIFY word. */
constexpr unsigned WORD_BITS = 16;
/** Largest sector count addressable with LBA28; larger disks are cut to this. */
constexpr uint32_t MAX_LBA28_SECTORS = 0x0FFFFFFF;
/** Most sectors in one read command. */
constexpr uint32_t MAX_READ_SECTORS = 255;
/** Most sectors in one write command. */
constexpr uint32_t MAX_WRITE_SECTORS = 255;

/** The master drive of the primary ATA channel. */
class Drive {
  public:
	/**
	 * @brief Requests the ports, disables drive interrupts and identifies the drive.
	 * @return true when a drive answered IDENTIFY with a non zero size; false when the
	 *         ports were refused (missing `IO_PORTS` capability), no drive is present,
	 *         the drive is not ATA, a wait timed out or it reported an error.
	 */
	bool init() {
		if (!cock::user::request_ports(COMMAND_BASE, COMMAND_PORT_COUNT) ||
			!cock::user::request_ports(CONTROL_PORT, CONTROL_PORT_COUNT))
			return false;

		cock::user::port_out8(CONTROL_PORT, CONTROL_INTERRUPTS_OFF);
		return identify();
	}

	/**
	 * @brief Returns the size of the drive.
	 * @return Number of 512 byte sectors (at most `MAX_LBA28_SECTORS`); zero before a
	 *         successful `init`.
	 */
	uint32_t sectors() const { return total; }

	/**
	 * @brief Reads sectors with PIO. Busy waits until the data is transferred.
	 * @param lba First sector.
	 * @param count Number of sectors, 1 to `MAX_READ_SECTORS`.
	 * @param buffer Destination of at least `count * 512` bytes.
	 * @return true when all sectors were read; false when the range is empty, too big or
	 *         outside the drive, or the drive reported an error or timed out (the
	 *         buffer may be partly filled).
	 */
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

	/**
	 * @brief Writes sectors with PIO and flushes the drive cache. Busy waits.
	 * @param lba First sector.
	 * @param count Number of sectors, 1 to `MAX_WRITE_SECTORS`.
	 * @param buffer Source of at least `count * 512` bytes.
	 * @return true when all sectors were written and the cache flush completed; false
	 *         when the range is empty, too big or outside the drive, or the drive
	 *         reported an error or timed out (the write may be partial).
	 */
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
	/** Size of the drive in sectors, set by `identify`. */
	uint32_t total = 0;

	/**
	 * @brief Loads the master drive, the sector count and the 28 bit address into the registers.
	 * @param lba First sector.
	 * @param count Number of sectors (the low byte is sent; 256 is not used).
	 */
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

	/**
	 * @brief Waits until the drive is not busy and checks that the command ended without error.
	 * @return true when not busy and neither error nor fault is set; false on timeout or error.
	 */
	static bool waitForCompletion() {
		settle();
		if (!waitNotBusy()) return false;
		uint8_t status = cock::user::port_in8(COMMAND_PORT);
		return (status & (STATUS_ERROR | STATUS_FAULT)) == 0;
	}

	/**
	 * @brief Sends the cache flush command and waits for it to complete.
	 * @return true when the flush completed without error.
	 */
	static bool flush() {
		cock::user::port_out8(COMMAND_PORT, COMMAND_CACHE_FLUSH);
		return waitForCompletion();
	}

	/** @brief Reads the alternate status `SETTLE_READS` times to give the drive time to react. */
	static void settle() {
		for (size_t read = 0; read < SETTLE_READS; read++)
			cock::user::port_in8(ALTERNATE_STATUS_PORT);
	}

	/**
	 * @brief Spins until the busy bit clears.
	 * @return true when the drive is not busy; false after `POLL_LIMIT` polls.
	 */
	static bool waitNotBusy() {
		for (uint32_t spin = 0; spin < POLL_LIMIT; spin++)
			if ((cock::user::port_in8(ALTERNATE_STATUS_PORT) & STATUS_BUSY) == 0)
				return true;
		return false;
	}

	/**
	 * @brief Waits until the drive offers a sector to transfer.
	 * @return true when not busy, no error or fault, and data request is set; false otherwise.
	 */
	static bool waitForData() {
		settle();
		if (!waitNotBusy()) return false;
		uint8_t status = cock::user::port_in8(COMMAND_PORT);
		return (status & (STATUS_ERROR | STATUS_FAULT)) == 0 &&
			   (status & STATUS_DATA_REQUEST) != 0;
	}

	/**
	 * @brief Sends IDENTIFY to the master and stores the sector count.
	 *
	 * Rejects a missing drive (status 0 or 0xFF) and non ATA devices (non zero LBA mid or
	 * high after the command). The count is cut to `MAX_LBA28_SECTORS`.
	 * @return true when the drive answered with a non zero sector count.
	 */
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
