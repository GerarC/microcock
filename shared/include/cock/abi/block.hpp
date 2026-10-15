#ifndef SHARED_COCK_ABI_BLOCK_HPP
#define SHARED_COCK_ABI_BLOCK_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::block {

constexpr const char *SERVER_NAME = "ata";

constexpr size_t SECTOR_BYTES = 512;
constexpr size_t WINDOW_PAGE_BYTES = 0x1000;
constexpr size_t WINDOW_PAGES = 1;
constexpr size_t WINDOW_BYTES = WINDOW_PAGES * WINDOW_PAGE_BYTES;
constexpr uint32_t MAX_SECTORS_PER_READ = WINDOW_BYTES / SECTOR_BYTES;

constexpr uint32_t DISK_DEVICE = 0;
constexpr uint32_t MAX_PARTITIONS = 16;
constexpr uint32_t MAX_DEVICES = DISK_DEVICE + 1 + MAX_PARTITIONS;
constexpr uint32_t NO_WINDOW = 0xFFFFFFFF;

enum class Status : uint32_t {
	OK = 0,
	NOT_CONNECTED = 1,
	BAD_REQUEST = 2,
	BAD_DEVICE = 3,
	OUT_OF_RANGE = 4,
	IO_ERROR = 5,
	NO_SPACE = 6,
};

enum class DeviceKind : uint32_t {
	DISK = 0,
	MBR_PARTITION = 1,
	GPT_PARTITION = 2,
};

static_assert(WINDOW_BYTES % SECTOR_BYTES == 0, "the window holds whole sectors");
static_assert(MAX_SECTORS_PER_READ > 0, "the window must hold a sector");

} // namespace cock::abi::block

#endif // !SHARED_COCK_ABI_BLOCK_HPP
