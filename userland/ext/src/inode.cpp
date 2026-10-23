#include <ext/inode.hpp>
#include <ext/stamp.hpp>
#include <ext/volume.hpp>
#include <string.h>

using cock::abi::ext2::Inode;
using cock::abi::ext2::InodeType;
using cock::abi::fs::FileType;

namespace {

bool groupLocation(uint32_t group, uint32_t &sector, uint32_t &offset) {
	using namespace cock::ext;
	if (group >= sb.groupCount) return false;
	uint32_t descriptor = group * static_cast<uint32_t>(cock::abi::ext2::GROUP_DESCRIPTOR_BYTES);
	sector = sb.groupTableSector() + descriptor / SECTOR_BYTES;
	offset = descriptor % SECTOR_BYTES;
	return true;
}

} // namespace

namespace cock::ext {

bool loadGroup(uint32_t group, uint32_t &offset) {
	uint32_t sector = 0;
	return groupLocation(group, sector, offset) && loadSector(sector, groupBuffer, groupLoaded);
}

bool inodeLocation(uint32_t number, uint32_t &sector, uint32_t &within) {
	if (number == 0 || number > sb.inodesCount) return false;

	uint32_t index = number - 1;
	uint32_t group = index / sb.inodesPerGroup;
	uint32_t inGroup = index % sb.inodesPerGroup;

	uint32_t offset = 0;
	if (!loadGroup(group, offset)) return false;
	uint32_t table = cock::abi::ext2::detail::readLe32(
		groupBuffer + offset + cock::abi::ext2::GD_INODE_TABLE_OFFSET);

	uint32_t byte = inGroup * sb.inodeBytes;
	if (!validBlock(table) || !validBlock(table + byte / sb.blockBytes)) return false;
	sector = table * sb.sectorsPerBlock() + byte / SECTOR_BYTES;
	within = byte % SECTOR_BYTES;
	return true;
}

bool loadInode(uint32_t number, Node &node) {
	uint32_t sector = 0;
	uint32_t within = 0;
	if (!inodeLocation(number, sector, within) || !loadSector(sector, inodeBuffer, inodeLoaded))
		return false;

	Inode inode = cock::abi::ext2::parseInode(inodeBuffer + within);
	if ((inode.flags & (cock::abi::ext2::INODE_EXTENTS_FLAG |
						cock::abi::ext2::INODE_INLINE_DATA_FLAG)) != 0)
		return false;

	node.inode = number;
	node.size = inode.size;
	node.links = inode.links;
	node.sectors = inode.sectors;
	memcpy(node.blocks, inode.blocks, sizeof(node.blocks));
	switch (inode.type) {
	case InodeType::REGULAR:
		node.type = FileType::REGULAR;
		return true;
	case InodeType::DIRECTORY:
		node.type = FileType::DIRECTORY;
		return true;
	case InodeType::SYMLINK:
		node.type = FileType::SYMLINK;
		return true;
	default:
		return false;
	}
}

bool storeInode(const Node &node, uint32_t newMode, bool deleted) {
	uint32_t sector = 0;
	uint32_t within = 0;
	if (!inodeLocation(node.inode, sector, within) ||
		!loadSector(sector, inodeBuffer, inodeLoaded))
		return false;

	uint32_t now = timestamp();
	uint8_t *raw = inodeBuffer + within;
	if (newMode != NOT_NEW) {
		memset(raw, 0, sb.inodeBytes);
		cock::abi::ext2::detail::writeLe16(raw + cock::abi::ext2::INODE_MODE_OFFSET, newMode);
		cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_ACCESS_TIME_OFFSET, now);
	}
	cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_SIZE_OFFSET, node.size);
	cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_CHANGE_TIME_OFFSET, now);
	cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_MODIFY_TIME_OFFSET, now);
	cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_DELETION_TIME_OFFSET,
									   deleted ? now : 0);
	cock::abi::ext2::detail::writeLe16(raw + cock::abi::ext2::INODE_LINKS_OFFSET, node.links);
	cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_SECTORS_OFFSET,
									   node.sectors);
	for (uint32_t slot = 0; slot < cock::abi::ext2::INODE_BLOCK_SLOTS; slot++)
		cock::abi::ext2::detail::writeLe32(
			raw + cock::abi::ext2::INODE_BLOCKS_OFFSET + size_t(slot) * sizeof(uint32_t),
			node.blocks[slot]);
	return storeSector(sector, inodeBuffer, inodeLoaded);
}

} // namespace cock::ext
