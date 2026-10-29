#include <ext/alloc.hpp>
#include <ext/blockmap.hpp>
#include <ext/inode.hpp>
#include <ext/volume.hpp>
#include <string.h>

using cock::abi::ext2::BlockPath;
using cock::abi::fs::Status;
using cock::ext::NO_BLOCK;
using cock::ext::Node;

namespace {

bool writePointer(uint32_t block, uint32_t index, uint32_t value) {
	using namespace cock::ext;
	uint32_t byte = index * static_cast<uint32_t>(sizeof(uint32_t));
	uint32_t sector = block * sb.sectorsPerBlock() + byte / SECTOR_BYTES;
	if (!loadSector(sector, pointerBuffer, pointerLoaded)) return false;
	cock::abi::ext2::detail::writeLe32(pointerBuffer + byte % SECTOR_BYTES, value);
	return storeSector(sector, pointerBuffer, pointerLoaded);
}

bool freeTree(uint32_t block, uint32_t depth) {
	using namespace cock::ext;
	if (!validBlock(block)) return false;

	bool clean = true;
	for (uint32_t index = 0; index < sb.pointersPerBlock(); index++) {
		uint32_t value = 0;
		if (!readPointer(block, index, value)) {
			clean = false;
			break;
		}
		if (value == NO_BLOCK) continue;
		clean = (depth > FIRST_LEVEL ? freeTree(value, depth - 1) : freeBlock(value)) && clean;
	}
	return freeBlock(block) && clean;
}

} // namespace

namespace cock::ext {

bool readPointer(uint32_t block, uint32_t index, uint32_t &value) {
	if (block == 0) {
		value = 0;
		return true;
	}
	if (!validBlock(block)) return false;

	uint32_t byte = index * static_cast<uint32_t>(sizeof(uint32_t));
	if (!loadSector(block * sb.sectorsPerBlock() + byte / SECTOR_BYTES, pointerBuffer,
					pointerLoaded))
		return false;
	value = cock::abi::ext2::detail::readLe32(pointerBuffer + byte % SECTOR_BYTES);
	return true;
}

bool physicalBlock(const Node &node, uint32_t fileBlock, uint32_t &physical) {
	BlockPath path = {};
	if (!cock::abi::ext2::blockPath(fileBlock, sb.pointersPerBlock(), path)) return false;

	uint32_t current = node.blocks[path.slots[0]];
	for (uint32_t level = 1; level <= path.depth; level++)
		if (!readPointer(current, path.slots[level], current)) return false;
	if (current != 0 && !validBlock(current)) return false;
	physical = current;
	return true;
}

Status mapForWrite(Node &node, uint32_t fileBlock, bool zeroNew, uint32_t &physical) {
	BlockPath path = {};
	if (!cock::abi::ext2::blockPath(fileBlock, sb.pointersPerBlock(), path))
		return Status::NO_SPACE;

	uint32_t group = (node.inode - 1) / sb.inodesPerGroup;
	uint32_t current = node.blocks[path.slots[0]];
	if (current == NO_BLOCK) {
		Status taken = allocateBlock(group, path.depth > 0 || zeroNew, current);
		if (taken != Status::OK) return taken;
		node.blocks[path.slots[0]] = current;
		node.sectors += sb.sectorsPerBlock();
	} else if (!validBlock(current)) {
		return Status::IO_ERROR;
	}

	for (uint32_t level = FIRST_LEVEL; level <= path.depth; level++) {
		uint32_t value = 0;
		if (!readPointer(current, path.slots[level], value)) return Status::IO_ERROR;
		if (value == NO_BLOCK) {
			Status taken = allocateBlock(group, level < path.depth || zeroNew, value);
			if (taken != Status::OK) return taken;
			if (!writePointer(current, path.slots[level], value)) return Status::IO_ERROR;
			node.sectors += sb.sectorsPerBlock();
		} else if (!validBlock(value)) {
			return Status::IO_ERROR;
		}
		current = value;
	}
	physical = current;
	return Status::OK;
}

bool releaseBlocks(const uint32_t *blocks) {
	bool clean = true;
	for (uint32_t slot = 0; slot < cock::abi::ext2::DIRECT_BLOCKS; slot++)
		if (blocks[slot] != NO_BLOCK) clean = freeBlock(blocks[slot]) && clean;

	const uint32_t slots[] = {cock::abi::ext2::SINGLE_INDIRECT_SLOT,
							  cock::abi::ext2::DOUBLE_INDIRECT_SLOT,
							  cock::abi::ext2::TRIPLE_INDIRECT_SLOT};
	uint32_t depth = FIRST_LEVEL;
	for (uint32_t slot : slots) {
		if (blocks[slot] != NO_BLOCK) clean = freeTree(blocks[slot], depth) && clean;
		depth++;
	}
	return clean;
}

Status emptyNode(Node &node, bool deleted) {
	uint32_t previous[cock::abi::ext2::INODE_BLOCK_SLOTS];
	memcpy(previous, node.blocks, sizeof(previous));
	Node saved = node;

	memset(node.blocks, 0, sizeof(node.blocks));
	node.size = NO_BYTES;
	node.sectors = NO_SECTORS;
	if (!storeInode(node, NOT_NEW, deleted)) {
		node = saved;
		return Status::IO_ERROR;
	}
	return releaseBlocks(previous) ? Status::OK : Status::IO_ERROR;
}

} // namespace cock::ext
