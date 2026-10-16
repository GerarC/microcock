/**
 * @file block.hpp
 * @brief Wire protocol of the block server (`ata`), shared by the driver and its clients.
 *
 * Requests are IPC messages (`BLK_CONNECT`, `BLK_INFO`, `BLK_READ`, `BLK_WRITE`) sent by a client to the
 * process named `SERVER_NAME`. `arg0` of every request is a request id chosen by the client; the server
 * answers with `BLK_REPLY` carrying the same id in `arg0`, a `Status` in `arg1` and up to three values in
 * `arg2` to `arg4`. Sector data travels through a one page shared memory window that the server creates on
 * `BLK_CONNECT`. Sectors are relative to the device, so a file system sees its partition from sector 0.
 */
#ifndef SHARED_COCK_ABI_BLOCK_HPP
#define SHARED_COCK_ABI_BLOCK_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::block {

/** Name under which the block server registers; clients find it with `SYS_PROC_LOOKUP`. */
constexpr const char *SERVER_NAME = "ata";

/** Size of one sector in bytes (logical sector of the ATA disk). */
constexpr size_t SECTOR_BYTES = 512;
/** Size of one page of the shared window in bytes. */
constexpr size_t WINDOW_PAGE_BYTES = 0x1000;
/** Pages of the shared window created per client session. */
constexpr size_t WINDOW_PAGES = 1;
/** Total size of the shared window in bytes (4096). */
constexpr size_t WINDOW_BYTES = WINDOW_PAGES * WINDOW_PAGE_BYTES;
/** Largest sector count accepted by `BLK_READ`: whole sectors that fit in the window (8). A count of 0 or more than this is `BAD_REQUEST`. */
constexpr uint32_t MAX_SECTORS_PER_READ = WINDOW_BYTES / SECTOR_BYTES;
/** Largest sector count accepted by `BLK_WRITE`; same limit as for reads. */
constexpr uint32_t MAX_SECTORS_PER_WRITE = MAX_SECTORS_PER_READ;

/** Device index of the whole disk. Partitions follow from index 1. Writes to this device are refused with `READ_ONLY`. */
constexpr uint32_t DISK_DEVICE = 0;
/** Most partitions the driver keeps in its device table. */
constexpr uint32_t MAX_PARTITIONS = 16;
/** Size of the device table: the disk plus all partitions (17). Valid device indexes are 0 to `MAX_DEVICES - 1`. */
constexpr uint32_t MAX_DEVICES = DISK_DEVICE + 1 + MAX_PARTITIONS;
/** Window id returned by `BLK_CONNECT` when the server could not create or share the window. */
constexpr uint32_t NO_WINDOW = 0xFFFFFFFF;

/** Result code carried in `arg1` of `BLK_REPLY`. */
enum class Status : uint32_t {
	/** The request succeeded; the values in the reply are valid. */
	OK = 0,
	/** The sender has no session (`BLK_CONNECT` was not done or the table dropped it). */
	NOT_CONNECTED = 1,
	/** Malformed request: sector count of 0 or above the window, or an unknown message. */
	BAD_REQUEST = 2,
	/** The device index is not in the device table. */
	BAD_DEVICE = 3,
	/** `sector + count` does not fit in the device (checked without wrapping). */
	OUT_OF_RANGE = 4,
	/** The drive did not answer within the poll limit or reported an error. */
	IO_ERROR = 5,
	/** A write could not be completed because the target is full. */
	NO_SPACE = 6,
	/** Write refused: the target is the whole disk (device 0). */
	READ_ONLY = 7,
};

/** Kind of a device, returned as the third value of `BLK_INFO`. */
enum class DeviceKind : uint32_t {
	/** The whole disk (device 0). */
	DISK = 0,
	/** A partition found in a Master Boot Record table. */
	MBR_PARTITION = 1,
	/** A partition found in a GUID Partition Table. */
	GPT_PARTITION = 2,
};

static_assert(WINDOW_BYTES % SECTOR_BYTES == 0, "the window holds whole sectors");
static_assert(MAX_SECTORS_PER_READ > 0, "the window must hold a sector");

} // namespace cock::abi::block

#endif // !SHARED_COCK_ABI_BLOCK_HPP
