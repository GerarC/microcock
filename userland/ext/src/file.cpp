#include <ext/blockmap.hpp>
#include <ext/file.hpp>
#include <ext/inode.hpp>
#include <ext/volume.hpp>
#include <string.h>

using cock::abi::fs::Status;

namespace cock::ext {

bool readBytes(const Node &node, uint8_t *destination, uint32_t offset, uint32_t count) {
	uint32_t done = 0;
	while (done < count) {
		uint32_t position = offset + done;
		uint32_t within = position % sb.blockBytes;
		uint32_t remaining = count - done;
		uint32_t physical = 0;
		if (!physicalBlock(node, position / sb.blockBytes, physical)) return false;

		if (physical == 0) {
			uint32_t take = sb.blockBytes - within;
			if (take > remaining) take = remaining;
			memset(destination + done, 0, take);
			done += take;
			continue;
		}

		uint32_t sectorInBlock = within / SECTOR_BYTES;
		uint32_t byteInSector = within % SECTOR_BYTES;
		uint32_t sector = physical * sb.sectorsPerBlock() + sectorInBlock;
		if (byteInSector == 0 && remaining >= SECTOR_BYTES) {
			uint32_t run = remaining / SECTOR_BYTES;
			if (run > sb.sectorsPerBlock() - sectorInBlock)
				run = sb.sectorsPerBlock() - sectorInBlock;
			if (run > cock::abi::block::MAX_SECTORS_PER_READ)
				run = cock::abi::block::MAX_SECTORS_PER_READ;
			if (!disk.read(device, sector, run, destination + done)) return false;
			done += run * static_cast<uint32_t>(SECTOR_BYTES);
		} else {
			if (!disk.read(device, sector, SINGLE_SECTOR, dataBuffer)) return false;
			uint32_t take = static_cast<uint32_t>(SECTOR_BYTES) - byteInSector;
			if (take > remaining) take = remaining;
			memcpy(destination + done, dataBuffer + byteInSector, take);
			done += take;
		}
	}
	return true;
}

Status writeBlocks(Node &node, const uint8_t *source, uint32_t offset, uint32_t count,
				   uint32_t &written) {
	const uint32_t originalSize = node.size;
	const uint32_t originalSectors = node.sectors;
	uint32_t done = 0;
	Status result = Status::OK;

	while (done < count) {
		uint32_t position = offset + done;
		uint32_t within = position % sb.blockBytes;
		uint32_t remaining = count - done;
		bool covers = within == 0 && remaining >= sb.blockBytes;
		uint32_t physical = 0;
		result = mapForWrite(node, position / sb.blockBytes, !covers, physical);
		if (result != Status::OK) break;

		uint32_t sectorInBlock = within / SECTOR_BYTES;
		uint32_t byteInSector = within % SECTOR_BYTES;
		uint32_t sector = physical * sb.sectorsPerBlock() + sectorInBlock;
		uint32_t take = 0;
		if (byteInSector == 0 && remaining >= SECTOR_BYTES) {
			uint32_t run = remaining / SECTOR_BYTES;
			if (run > sb.sectorsPerBlock() - sectorInBlock)
				run = sb.sectorsPerBlock() - sectorInBlock;
			if (run > cock::abi::block::MAX_SECTORS_PER_WRITE)
				run = cock::abi::block::MAX_SECTORS_PER_WRITE;
			if (!disk.write(device, sector, run, source + done)) {
				result = Status::IO_ERROR;
				break;
			}
			take = run * static_cast<uint32_t>(SECTOR_BYTES);
		} else {
			if (!disk.read(device, sector, SINGLE_SECTOR, dataBuffer)) {
				result = Status::IO_ERROR;
				break;
			}
			take = static_cast<uint32_t>(SECTOR_BYTES) - byteInSector;
			if (take > remaining) take = remaining;
			memcpy(dataBuffer + byteInSector, source + done, take);
			if (!disk.write(device, sector, SINGLE_SECTOR, dataBuffer)) {
				result = Status::IO_ERROR;
				break;
			}
		}
		done += take;
	}

	written = done;
	if (offset + done > node.size) node.size = offset + done;
	if ((node.size != originalSize || node.sectors != originalSectors) && !storeInode(node))
		result = Status::IO_ERROR;
	if (done > 0 && result == Status::NO_SPACE) result = Status::OK;
	return result;
}

} // namespace cock::ext
