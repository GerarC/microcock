#include <ext/alloc.hpp>
#include <ext/inode.hpp>
#include <ext/volume.hpp>

using cock::abi::fs::Status;

namespace {

bool loadBitmapSector(uint32_t bitmapBlock, uint32_t index) {
	using namespace cock::ext;
	return loadSector(bitmapBlock * sb.sectorsPerBlock() + index, bitmapBuffer, bitmapLoaded);
}

bool zeroBlock(uint32_t block) {
	using namespace cock::ext;
	pointerLoaded = NOTHING_LOADED;
	directoryLoaded = NOTHING_LOADED;
	return disk.write(device, block * sb.sectorsPerBlock(), sb.sectorsPerBlock(), zeros);
}

Status takeBit(uint32_t group, uint32_t bitmapOffset, uint32_t total, uint32_t &bit) {
	using namespace cock::ext;
	uint32_t offset = 0;
	if (!loadGroup(group, offset)) return Status::IO_ERROR;
	uint32_t bitmapBlock = cock::abi::ext2::detail::readLe32(groupBuffer + offset + bitmapOffset);
	if (!validBlock(bitmapBlock)) return Status::IO_ERROR;

	for (uint32_t base = 0; base < total; base += cock::abi::ext2::BITS_PER_BITMAP_SECTOR) {
		uint32_t limit = total - base < cock::abi::ext2::BITS_PER_BITMAP_SECTOR
							 ? total - base
							 : cock::abi::ext2::BITS_PER_BITMAP_SECTOR;
		if (!loadBitmapSector(bitmapBlock, base / cock::abi::ext2::BITS_PER_BITMAP_SECTOR))
			return Status::IO_ERROR;

		uint32_t found = cock::abi::ext2::findClearBit(bitmapBuffer, 0, limit);
		if (found == cock::abi::ext2::NO_BIT) continue;

		cock::abi::ext2::setBit(bitmapBuffer, found);
		if (!storeSector(bitmapLoaded, bitmapBuffer, bitmapLoaded)) return Status::IO_ERROR;
		bit = base + found;
		return Status::OK;
	}
	return Status::NO_SPACE;
}

bool returnBit(uint32_t group, uint32_t bitmapOffset, uint32_t bit) {
	using namespace cock::ext;
	uint32_t offset = 0;
	if (!loadGroup(group, offset)) return false;
	uint32_t bitmapBlock = cock::abi::ext2::detail::readLe32(groupBuffer + offset + bitmapOffset);
	if (!validBlock(bitmapBlock)) return false;

	if (!loadBitmapSector(bitmapBlock, bit / cock::abi::ext2::BITS_PER_BITMAP_SECTOR))
		return false;
	uint32_t within = bit % cock::abi::ext2::BITS_PER_BITMAP_SECTOR;
	if (!cock::abi::ext2::bitIsSet(bitmapBuffer, within)) return false;

	cock::abi::ext2::clearBit(bitmapBuffer, within);
	return storeSector(bitmapLoaded, bitmapBuffer, bitmapLoaded);
}

} // namespace

namespace cock::ext {

bool adjustGroup(uint32_t group, int32_t blocks, int32_t inodes, int32_t directories) {
	uint32_t offset = 0;
	if (!loadGroup(group, offset)) return false;

	uint8_t *raw = groupBuffer + offset;
	if (directories != 0) {
		uint32_t used =
			cock::abi::ext2::detail::readLe16(raw + cock::abi::ext2::GD_USED_DIRECTORIES_OFFSET);
		cock::abi::ext2::detail::writeLe16(
			raw + cock::abi::ext2::GD_USED_DIRECTORIES_OFFSET,
			static_cast<uint32_t>(static_cast<int32_t>(used) + directories));
	}
	uint32_t freeBlocks =
		cock::abi::ext2::detail::readLe16(raw + cock::abi::ext2::GD_FREE_BLOCKS_OFFSET);
	uint32_t freeInodes =
		cock::abi::ext2::detail::readLe16(raw + cock::abi::ext2::GD_FREE_INODES_OFFSET);
	cock::abi::ext2::detail::writeLe16(
		raw + cock::abi::ext2::GD_FREE_BLOCKS_OFFSET,
		static_cast<uint32_t>(static_cast<int32_t>(freeBlocks) + blocks));
	cock::abi::ext2::detail::writeLe16(
		raw + cock::abi::ext2::GD_FREE_INODES_OFFSET,
		static_cast<uint32_t>(static_cast<int32_t>(freeInodes) + inodes));
	return storeSector(groupLoaded, groupBuffer, groupLoaded);
}

Status allocateBlock(uint32_t preferred, bool zero, uint32_t &block) {
	if (sb.freeBlocks == 0) return Status::NO_SPACE;

	for (uint32_t tried = 0; tried < sb.groupCount; tried++) {
		uint32_t group = (preferred + tried) % sb.groupCount;
		uint32_t offset = 0;
		if (!loadGroup(group, offset)) return Status::IO_ERROR;
		if (cock::abi::ext2::detail::readLe16(groupBuffer + offset +
											  cock::abi::ext2::GD_FREE_BLOCKS_OFFSET) == 0)
			continue;

		uint32_t first = sb.firstDataBlock + group * sb.blocksPerGroup;
		uint32_t inGroup = sb.blocksCount - first < sb.blocksPerGroup ? sb.blocksCount - first
																	  : sb.blocksPerGroup;
		uint32_t bit = 0;
		Status taken = takeBit(group, cock::abi::ext2::GD_BLOCK_BITMAP_OFFSET, inGroup, bit);
		if (taken == Status::NO_SPACE) continue;
		if (taken != Status::OK) return taken;

		if (!adjustGroup(group, -1, 0)) return Status::IO_ERROR;
		sb.freeBlocks--;
		block = first + bit;
		if (zero && !zeroBlock(block)) return Status::IO_ERROR;
		return Status::OK;
	}
	return Status::NO_SPACE;
}

bool freeBlock(uint32_t block) {
	if (!validBlock(block)) return false;

	uint32_t relative = block - sb.firstDataBlock;
	uint32_t group = relative / sb.blocksPerGroup;
	if (!returnBit(group, cock::abi::ext2::GD_BLOCK_BITMAP_OFFSET,
				   relative % sb.blocksPerGroup) ||
		!adjustGroup(group, 1, 0))
		return false;

	sb.freeBlocks++;
	pointerLoaded = NOTHING_LOADED;
	directoryLoaded = NOTHING_LOADED;
	return true;
}

Status allocateInode(uint32_t preferred, uint32_t &number) {
	if (sb.freeInodes == 0) return Status::NO_SPACE;

	for (uint32_t tried = 0; tried < sb.groupCount; tried++) {
		uint32_t group = (preferred + tried) % sb.groupCount;
		uint32_t offset = 0;
		if (!loadGroup(group, offset)) return Status::IO_ERROR;
		if (cock::abi::ext2::detail::readLe16(groupBuffer + offset +
											  cock::abi::ext2::GD_FREE_INODES_OFFSET) == 0)
			continue;

		uint32_t first = group * sb.inodesPerGroup;
		if (first >= sb.inodesCount) continue;
		uint32_t inGroup = sb.inodesCount - first < sb.inodesPerGroup ? sb.inodesCount - first
																	  : sb.inodesPerGroup;
		uint32_t bit = 0;
		Status taken = takeBit(group, cock::abi::ext2::GD_INODE_BITMAP_OFFSET, inGroup, bit);
		if (taken == Status::NO_SPACE) continue;
		if (taken != Status::OK) return taken;

		if (!adjustGroup(group, 0, -1)) return Status::IO_ERROR;
		sb.freeInodes--;
		number = first + bit + 1;
		return Status::OK;
	}
	return Status::NO_SPACE;
}

bool freeInode(uint32_t number) {
	uint32_t index = number - 1;
	uint32_t group = index / sb.inodesPerGroup;
	if (!returnBit(group, cock::abi::ext2::GD_INODE_BITMAP_OFFSET, index % sb.inodesPerGroup) ||
		!adjustGroup(group, 0, 1))
		return false;

	sb.freeInodes++;
	return true;
}

} // namespace cock::ext
