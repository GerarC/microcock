#include <cock/abi/block.hpp>
#include <cock/abi/datetime.hpp>
#include <cock/abi/ext2.hpp>
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/block.hpp>
#include <cock/user/clock.hpp>
#include <cock/user/file_server.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using cock::abi::ext2::BlockPath;
using cock::abi::ext2::DirItem;
using cock::abi::ext2::Inode;
using cock::abi::ext2::InodeType;
using cock::abi::ext2::Superblock;
using cock::abi::fs::FileType;
using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;
constexpr uint32_t NOTHING_LOADED = 0xFFFFFFFF;
constexpr uint32_t SINGLE_SECTOR = 1;
constexpr size_t SECTOR_BYTES = cock::abi::ext2::SECTOR_BYTES;
constexpr size_t MAX_NAME_BYTES = 255;
constexpr size_t MAX_LINK_BYTES = 255;
constexpr size_t WORK_BYTES = 512;
constexpr uint32_t MAX_LINKS = 8;
constexpr uint32_t NO_BYTES = 0;
constexpr uint32_t NO_BLOCK = 0;
constexpr uint32_t NO_LINKS = 0;
constexpr uint32_t NO_SECTORS = 0;
constexpr uint32_t FIRST_LEVEL = 1;
constexpr uint32_t NOT_NEW = 0;
constexpr uint32_t MAX_DEPTH = 256;
constexpr char PATH_SEPARATOR = '/';
constexpr char CURRENT_NAME[] = ".";
constexpr char PARENT_NAME[] = "..";

struct Where {
	uint32_t physical;
	uint32_t offset;
	uint32_t previous;
	bool hasPrevious;
	uint32_t inode;
	uint32_t recordBytes;
};

struct Ext {
	struct Node {
		uint32_t inode;
		uint32_t size;
		FileType type;
		uint32_t links;
		uint32_t sectors;
		uint32_t blocks[cock::abi::ext2::INODE_BLOCK_SLOTS];
	};

	struct Handle {
		bool open;
		Node node;
	};

	static constexpr uint32_t MAX_FILE_BYTES = cock::abi::ext2::MAX_FILE_BYTES;

	inline static cock::user::block::Client disk;
	inline static uint32_t device = 0;
	inline static bool readOnly = true;
	inline static cock::user::Clock clock;
	inline static bool clockReady = false;
	inline static Superblock sb = {};
	inline static uint8_t superblockBuffer[cock::abi::ext2::SUPERBLOCK_BYTES];
	inline static uint8_t groupBuffer[SECTOR_BYTES];
	inline static uint32_t groupLoaded = NOTHING_LOADED;
	inline static uint8_t inodeBuffer[SECTOR_BYTES];
	inline static uint32_t inodeLoaded = NOTHING_LOADED;
	inline static uint8_t pointerBuffer[SECTOR_BYTES];
	inline static uint32_t pointerLoaded = NOTHING_LOADED;
	inline static uint8_t bitmapBuffer[SECTOR_BYTES];
	inline static uint32_t bitmapLoaded = NOTHING_LOADED;
	inline static uint8_t directoryBuffer[cock::abi::ext2::MAX_BLOCK_BYTES];
	inline static uint32_t directoryLoaded = NOTHING_LOADED;
	inline static uint8_t dataBuffer[SECTOR_BYTES];
	inline static uint8_t zeros[cock::abi::ext2::MAX_BLOCK_BYTES];

	static bool writable() { return sb.writable && !readOnly; }

	static bool sameFile(const Node &first, const Node &second) {
		return first.inode == second.inode;
	}

	static bool validBlock(uint32_t block) {
		return block >= sb.firstDataBlock && block < sb.blocksCount;
	}

	static bool loadSector(uint32_t sector, uint8_t *buffer, uint32_t &loaded) {
		if (sector == loaded) return true;
		loaded = NOTHING_LOADED;
		if (!disk.read(device, sector, SINGLE_SECTOR, buffer)) return false;
		loaded = sector;
		return true;
	}

	static bool storeSector(uint32_t sector, const uint8_t *buffer, uint32_t &loaded) {
		if (disk.write(device, sector, SINGLE_SECTOR, buffer)) return true;
		loaded = NOTHING_LOADED;
		return false;
	}

	static bool groupLocation(uint32_t group, uint32_t &sector, uint32_t &offset) {
		if (group >= sb.groupCount) return false;
		uint32_t descriptor =
			group * static_cast<uint32_t>(cock::abi::ext2::GROUP_DESCRIPTOR_BYTES);
		sector = sb.groupTableSector() + descriptor / SECTOR_BYTES;
		offset = descriptor % SECTOR_BYTES;
		return true;
	}

	static bool loadGroup(uint32_t group, uint32_t &offset) {
		uint32_t sector = 0;
		return groupLocation(group, sector, offset) &&
			   loadSector(sector, groupBuffer, groupLoaded);
	}

	static bool inodeLocation(uint32_t number, uint32_t &sector, uint32_t &within) {
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

	static bool loadInode(uint32_t number, Node &node) {
		uint32_t sector = 0;
		uint32_t within = 0;
		if (!inodeLocation(number, sector, within) ||
			!loadSector(sector, inodeBuffer, inodeLoaded))
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

	static bool readPointer(uint32_t block, uint32_t index, uint32_t &value) {
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

	static bool physicalBlock(const Node &node, uint32_t fileBlock, uint32_t &physical) {
		BlockPath path = {};
		if (!cock::abi::ext2::blockPath(fileBlock, sb.pointersPerBlock(), path)) return false;

		uint32_t current = node.blocks[path.slots[0]];
		for (uint32_t level = 1; level <= path.depth; level++)
			if (!readPointer(current, path.slots[level], current)) return false;
		if (current != 0 && !validBlock(current)) return false;
		physical = current;
		return true;
	}

	static bool readBytes(const Node &node, uint8_t *destination, uint32_t offset,
						  uint32_t count) {
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

	static bool loadDirectoryBlock(uint32_t physical) {
		if (physical == directoryLoaded) return true;
		directoryLoaded = NOTHING_LOADED;
		if (!disk.read(device, physical * sb.sectorsPerBlock(), sb.sectorsPerBlock(),
					   directoryBuffer))
			return false;
		directoryLoaded = physical;
		return true;
	}

	static bool storeDirectoryBlock(uint32_t physical) {
		if (disk.write(device, physical * sb.sectorsPerBlock(), sb.sectorsPerBlock(),
					   directoryBuffer)) {
			directoryLoaded = physical;
			return true;
		}
		directoryLoaded = NOTHING_LOADED;
		return false;
	}

	static bool nextEntry(const Node &directory, uint32_t &offset, DirItem &item) {
		for (;;) {
			if (offset >= directory.size) return false;

			uint32_t fileBlock = offset / sb.blockBytes;
			uint32_t within = offset % sb.blockBytes;
			uint32_t nextBlock = (fileBlock + 1) * sb.blockBytes;
			uint32_t physical = 0;
			if (!physicalBlock(directory, fileBlock, physical)) return false;
			if (physical == 0) {
				offset = nextBlock;
				continue;
			}

			if (!loadDirectoryBlock(physical)) return false;

			if (!cock::abi::ext2::parseDirItem(directoryBuffer, sb.blockBytes, within,
											   sb.fileType, item)) {
				offset = nextBlock;
				continue;
			}
			offset += item.recordBytes;
			if (item.inode != 0 && item.nameBytes != 0) return true;
		}
	}

	static bool findEntry(const Node &directory, const char *name, uint32_t &inode) {
		size_t length = strlen(name);
		uint32_t offset = 0;
		DirItem item = {};
		while (nextEntry(directory, offset, item)) {
			if (item.nameBytes != length || memcmp(item.name, name, length) != 0) continue;
			inode = item.inode;
			return true;
		}
		return false;
	}

	static bool readLink(const Node &link, char *target, size_t capacity) {
		if (link.size == 0 || link.size >= capacity) return false;

		if (link.size < cock::abi::ext2::FAST_SYMLINK_BYTES)
			memcpy(target, link.blocks, link.size);
		else if (!readBytes(link, reinterpret_cast<uint8_t *>(target), 0, link.size))
			return false;
		target[link.size] = '\0';
		return true;
	}

	static bool resolve(const char *path, Node &result) {
		char work[WORK_BYTES];
		size_t length = strlen(path);
		if (length >= sizeof(work)) return false;
		memcpy(work, path, length + 1);

		Node root = {};
		if (!loadInode(cock::abi::ext2::ROOT_INODE, root) || root.type != FileType::DIRECTORY)
			return false;

		Node current = root;
		const char *cursor = work;
		uint32_t links = 0;
		for (;;) {
			while (*cursor == PATH_SEPARATOR)
				cursor++;
			if (*cursor == '\0') break;

			char component[MAX_NAME_BYTES + 1];
			size_t used = 0;
			while (*cursor != '\0' && *cursor != PATH_SEPARATOR) {
				if (used >= MAX_NAME_BYTES) return false;
				component[used++] = *cursor++;
			}
			component[used] = '\0';

			uint32_t inode = 0;
			Node next = {};
			if (current.type != FileType::DIRECTORY || !findEntry(current, component, inode) ||
				!loadInode(inode, next))
				return false;

			if (next.type != FileType::SYMLINK) {
				current = next;
				continue;
			}

			char target[MAX_LINK_BYTES + 1];
			if (++links > MAX_LINKS || !readLink(next, target, sizeof(target))) return false;
			size_t targetLength = strlen(target);
			size_t restLength = strlen(cursor);
			if (targetLength + 1 + restLength >= WORK_BYTES) return false;

			char combined[WORK_BYTES];
			memcpy(combined, target, targetLength);
			combined[targetLength] = PATH_SEPARATOR;
			memcpy(combined + targetLength + 1, cursor, restLength + 1);
			memcpy(work, combined, targetLength + 1 + restLength + 1);
			cursor = work;
			if (target[0] == PATH_SEPARATOR) current = root;
		}
		result = current;
		return true;
	}

	static bool read(Handle &handle, uint8_t *destination, uint32_t offset, uint32_t count) {
		return readBytes(handle.node, destination, offset, count);
	}

	static bool list(const Node &directory, uint32_t wanted, char *name, size_t capacity,
					 FileType &type) {
		uint32_t seen = 0;
		uint32_t offset = 0;
		DirItem item = {};
		while (nextEntry(directory, offset, item)) {
			if (item.nameBytes >= capacity) continue;
			if ((item.nameBytes == strlen(CURRENT_NAME) &&
				 memcmp(item.name, CURRENT_NAME, item.nameBytes) == 0) ||
				(item.nameBytes == strlen(PARENT_NAME) &&
				 memcmp(item.name, PARENT_NAME, item.nameBytes) == 0))
				continue;

			Node entry = {};
			if (!loadInode(item.inode, entry)) continue;
			if (seen++ != wanted) continue;

			memcpy(name, item.name, item.nameBytes);
			name[item.nameBytes] = '\0';
			type = entry.type;
			return true;
		}
		return false;
	}

	static bool flushSuperblock(bool valid) {
		uint32_t state = valid ? (sb.state | cock::abi::ext2::STATE_VALID)
							   : (sb.state & ~cock::abi::ext2::STATE_VALID);
		cock::abi::ext2::detail::writeLe32(
			superblockBuffer + cock::abi::ext2::SB_FREE_BLOCKS_OFFSET, sb.freeBlocks);
		cock::abi::ext2::detail::writeLe32(
			superblockBuffer + cock::abi::ext2::SB_FREE_INODES_OFFSET, sb.freeInodes);
		cock::abi::ext2::detail::writeLe16(
			superblockBuffer + cock::abi::ext2::SB_STATE_OFFSET, state);
		return disk.write(device, cock::abi::ext2::SUPERBLOCK_SECTOR, SINGLE_SECTOR,
						  superblockBuffer);
	}

	static bool begin() { return flushSuperblock(false); }

	static Status end(Status status) {
		return flushSuperblock(true) ? status : Status::IO_ERROR;
	}

	static uint32_t timestamp() {
		cock::abi::datetime::DateTime now = {};
		if (clockReady && clock.now(now)) return cock::abi::datetime::toUnix(now);
		return cock::abi::ext2::DELETION_TIME_MARK;
	}

	static bool adjustGroup(uint32_t group, int32_t blocks, int32_t inodes,
							int32_t directories = 0) {
		uint32_t offset = 0;
		if (!loadGroup(group, offset)) return false;

		uint8_t *raw = groupBuffer + offset;
		if (directories != 0) {
			uint32_t used = cock::abi::ext2::detail::readLe16(
				raw + cock::abi::ext2::GD_USED_DIRECTORIES_OFFSET);
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

	static bool loadBitmapSector(uint32_t bitmapBlock, uint32_t index) {
		return loadSector(bitmapBlock * sb.sectorsPerBlock() + index, bitmapBuffer,
						  bitmapLoaded);
	}

	static bool zeroBlock(uint32_t block) {
		pointerLoaded = NOTHING_LOADED;
		directoryLoaded = NOTHING_LOADED;
		return disk.write(device, block * sb.sectorsPerBlock(), sb.sectorsPerBlock(), zeros);
	}

	static Status takeBit(uint32_t group, uint32_t bitmapOffset, uint32_t total,
						  uint32_t &bit) {
		uint32_t offset = 0;
		if (!loadGroup(group, offset)) return Status::IO_ERROR;
		uint32_t bitmapBlock =
			cock::abi::ext2::detail::readLe32(groupBuffer + offset + bitmapOffset);
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

	static bool returnBit(uint32_t group, uint32_t bitmapOffset, uint32_t bit) {
		uint32_t offset = 0;
		if (!loadGroup(group, offset)) return false;
		uint32_t bitmapBlock =
			cock::abi::ext2::detail::readLe32(groupBuffer + offset + bitmapOffset);
		if (!validBlock(bitmapBlock)) return false;

		if (!loadBitmapSector(bitmapBlock, bit / cock::abi::ext2::BITS_PER_BITMAP_SECTOR))
			return false;
		uint32_t within = bit % cock::abi::ext2::BITS_PER_BITMAP_SECTOR;
		if (!cock::abi::ext2::bitIsSet(bitmapBuffer, within)) return false;

		cock::abi::ext2::clearBit(bitmapBuffer, within);
		return storeSector(bitmapLoaded, bitmapBuffer, bitmapLoaded);
	}

	static Status allocateBlock(uint32_t preferred, bool zero, uint32_t &block) {
		if (sb.freeBlocks == 0) return Status::NO_SPACE;

		for (uint32_t tried = 0; tried < sb.groupCount; tried++) {
			uint32_t group = (preferred + tried) % sb.groupCount;
			uint32_t offset = 0;
			if (!loadGroup(group, offset)) return Status::IO_ERROR;
			if (cock::abi::ext2::detail::readLe16(
					groupBuffer + offset + cock::abi::ext2::GD_FREE_BLOCKS_OFFSET) == 0)
				continue;

			uint32_t first = sb.firstDataBlock + group * sb.blocksPerGroup;
			uint32_t inGroup = sb.blocksCount - first < sb.blocksPerGroup
								   ? sb.blocksCount - first
								   : sb.blocksPerGroup;
			uint32_t bit = 0;
			Status taken =
				takeBit(group, cock::abi::ext2::GD_BLOCK_BITMAP_OFFSET, inGroup, bit);
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

	static bool freeBlock(uint32_t block) {
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

	static Status allocateInode(uint32_t preferred, uint32_t &number) {
		if (sb.freeInodes == 0) return Status::NO_SPACE;

		for (uint32_t tried = 0; tried < sb.groupCount; tried++) {
			uint32_t group = (preferred + tried) % sb.groupCount;
			uint32_t offset = 0;
			if (!loadGroup(group, offset)) return Status::IO_ERROR;
			if (cock::abi::ext2::detail::readLe16(
					groupBuffer + offset + cock::abi::ext2::GD_FREE_INODES_OFFSET) == 0)
				continue;

			uint32_t first = group * sb.inodesPerGroup;
			if (first >= sb.inodesCount) continue;
			uint32_t inGroup = sb.inodesCount - first < sb.inodesPerGroup
								   ? sb.inodesCount - first
								   : sb.inodesPerGroup;
			uint32_t bit = 0;
			Status taken =
				takeBit(group, cock::abi::ext2::GD_INODE_BITMAP_OFFSET, inGroup, bit);
			if (taken == Status::NO_SPACE) continue;
			if (taken != Status::OK) return taken;

			if (!adjustGroup(group, 0, -1)) return Status::IO_ERROR;
			sb.freeInodes--;
			number = first + bit + 1;
			return Status::OK;
		}
		return Status::NO_SPACE;
	}

	static bool freeInode(uint32_t number) {
		uint32_t index = number - 1;
		uint32_t group = index / sb.inodesPerGroup;
		if (!returnBit(group, cock::abi::ext2::GD_INODE_BITMAP_OFFSET,
					   index % sb.inodesPerGroup) ||
			!adjustGroup(group, 0, 1))
			return false;

		sb.freeInodes++;
		return true;
	}

	static bool storeInode(const Node &node, uint32_t newMode = NOT_NEW, bool deleted = false) {
		uint32_t sector = 0;
		uint32_t within = 0;
		if (!inodeLocation(node.inode, sector, within) ||
			!loadSector(sector, inodeBuffer, inodeLoaded))
			return false;

		uint32_t now = timestamp();
		uint8_t *raw = inodeBuffer + within;
		if (newMode != NOT_NEW) {
			memset(raw, 0, sb.inodeBytes);
			cock::abi::ext2::detail::writeLe16(raw + cock::abi::ext2::INODE_MODE_OFFSET,
											   newMode);
			cock::abi::ext2::detail::writeLe32(
				raw + cock::abi::ext2::INODE_ACCESS_TIME_OFFSET, now);
		}
		cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_SIZE_OFFSET,
										   node.size);
		cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_CHANGE_TIME_OFFSET,
										   now);
		cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_MODIFY_TIME_OFFSET,
										   now);
		cock::abi::ext2::detail::writeLe32(
			raw + cock::abi::ext2::INODE_DELETION_TIME_OFFSET, deleted ? now : 0);
		cock::abi::ext2::detail::writeLe16(raw + cock::abi::ext2::INODE_LINKS_OFFSET,
										   node.links);
		cock::abi::ext2::detail::writeLe32(raw + cock::abi::ext2::INODE_SECTORS_OFFSET,
										   node.sectors);
		for (uint32_t slot = 0; slot < cock::abi::ext2::INODE_BLOCK_SLOTS; slot++)
			cock::abi::ext2::detail::writeLe32(
				raw + cock::abi::ext2::INODE_BLOCKS_OFFSET + size_t(slot) * sizeof(uint32_t),
				node.blocks[slot]);
		return storeSector(sector, inodeBuffer, inodeLoaded);
	}

	static bool writePointer(uint32_t block, uint32_t index, uint32_t value) {
		uint32_t byte = index * static_cast<uint32_t>(sizeof(uint32_t));
		uint32_t sector = block * sb.sectorsPerBlock() + byte / SECTOR_BYTES;
		if (!loadSector(sector, pointerBuffer, pointerLoaded)) return false;
		cock::abi::ext2::detail::writeLe32(pointerBuffer + byte % SECTOR_BYTES, value);
		return storeSector(sector, pointerBuffer, pointerLoaded);
	}

	static Status mapForWrite(Node &node, uint32_t fileBlock, bool zeroNew,
							  uint32_t &physical) {
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

	static bool freeTree(uint32_t block, uint32_t depth) {
		if (!validBlock(block)) return false;

		bool clean = true;
		for (uint32_t index = 0; index < sb.pointersPerBlock(); index++) {
			uint32_t value = 0;
			if (!readPointer(block, index, value)) {
				clean = false;
				break;
			}
			if (value == NO_BLOCK) continue;
			clean = (depth > FIRST_LEVEL ? freeTree(value, depth - 1) : freeBlock(value)) &&
					clean;
		}
		return freeBlock(block) && clean;
	}

	static bool releaseBlocks(const uint32_t *blocks) {
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

	static Status emptyNode(Node &node, bool deleted = false) {
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

	static bool locateEntry(const Node &directory, const char *name, Where &where) {
		size_t length = strlen(name);
		for (uint32_t fileBlock = 0; fileBlock * sb.blockBytes < directory.size; fileBlock++) {
			uint32_t physical = 0;
			if (!physicalBlock(directory, fileBlock, physical)) return false;
			if (physical == 0 || !loadDirectoryBlock(physical)) continue;

			uint32_t offset = 0;
			uint32_t previous = 0;
			bool hasPrevious = false;
			DirItem item = {};
			while (cock::abi::ext2::parseDirItem(directoryBuffer, sb.blockBytes, offset,
												 sb.fileType, item)) {
				if (item.inode != 0 && item.nameBytes == length &&
					memcmp(item.name, name, length) == 0) {
					where = {physical, offset, previous, hasPrevious, item.inode,
							 item.recordBytes};
					return true;
				}
				previous = offset;
				hasPrevious = true;
				offset += item.recordBytes;
			}
		}
		return false;
	}

	static Status addEntry(Node &directory, const char *name, uint32_t nameBytes,
						   uint32_t inode, uint8_t type) {
		uint32_t need = cock::abi::ext2::direntBytes(nameBytes);
		uint32_t blocks = directory.size / sb.blockBytes;
		for (uint32_t fileBlock = 0; fileBlock < blocks; fileBlock++) {
			uint32_t physical = 0;
			if (!physicalBlock(directory, fileBlock, physical)) return Status::IO_ERROR;
			if (physical == 0) continue;
			if (!loadDirectoryBlock(physical)) return Status::IO_ERROR;

			uint32_t offset = 0;
			DirItem item = {};
			while (cock::abi::ext2::parseDirItem(directoryBuffer, sb.blockBytes, offset,
												 sb.fileType, item)) {
				uint32_t used = item.inode == 0 ? 0 : cock::abi::ext2::direntBytes(item.nameBytes);
				if (item.recordBytes - used >= need) {
					if (used != 0)
						cock::abi::ext2::detail::writeLe16(
							directoryBuffer + offset + cock::abi::ext2::DIRENT_RECORD_OFFSET,
							used);
					cock::abi::ext2::encodeDirItem(
						directoryBuffer + offset + used, inode, item.recordBytes - used, name,
						nameBytes, sb.fileType, type);
					return storeDirectoryBlock(physical) ? Status::OK : Status::IO_ERROR;
				}
				offset += item.recordBytes;
			}
		}

		uint32_t physical = 0;
		uint32_t sectors = directory.sectors;
		Status mapped = mapForWrite(directory, blocks, false, physical);
		if (mapped != Status::OK) {
			if (directory.sectors != sectors) storeInode(directory);
			return mapped;
		}

		memset(directoryBuffer, 0, sb.blockBytes);
		cock::abi::ext2::encodeDirItem(directoryBuffer, inode, sb.blockBytes, name, nameBytes,
									   sb.fileType, type);
		if (!storeDirectoryBlock(physical)) return Status::IO_ERROR;

		directory.size += sb.blockBytes;
		return storeInode(directory) ? Status::OK : Status::IO_ERROR;
	}

	static bool parentAndName(const char *path, Node &parent, char *name, size_t &nameBytes) {
		char parentPath[cock::abi::fs::MAX_PATH_BYTES];
		const char *leaf = nullptr;
		if (!cock::abi::fs::splitPath(path, parentPath, sizeof(parentPath), leaf, nameBytes) ||
			nameBytes > MAX_NAME_BYTES)
			return false;

		memcpy(name, leaf, nameBytes);
		name[nameBytes] = '\0';
		return resolve(parentPath, parent) && parent.type == FileType::DIRECTORY;
	}

	static bool resolveLeaf(const char *path, Node &node) {
		Node parent = {};
		char name[MAX_NAME_BYTES + 1];
		size_t nameBytes = 0;
		if (!parentAndName(path, parent, name, nameBytes)) return false;

		uint32_t inode = 0;
		return findEntry(parent, name, inode) && loadInode(inode, node);
	}

	static Status createFile(const char *path, Node &node) {
		Node parent = {};
		char name[MAX_NAME_BYTES + 1];
		size_t nameBytes = 0;
		if (!parentAndName(path, parent, name, nameBytes)) return Status::NOT_FOUND;
		if (strcmp(name, CURRENT_NAME) == 0 || strcmp(name, PARENT_NAME) == 0)
			return Status::BAD_REQUEST;

		uint32_t existing = 0;
		if (findEntry(parent, name, existing)) return Status::EXISTS;

		uint32_t number = 0;
		Status taken = allocateInode((parent.inode - 1) / sb.inodesPerGroup, number);
		if (taken != Status::OK) return taken;

		Node fresh = {};
		fresh.inode = number;
		fresh.type = FileType::REGULAR;
		fresh.links = cock::abi::ext2::NEW_FILE_LINKS;
		if (!storeInode(fresh, cock::abi::ext2::MODE_NEW_FILE)) return Status::IO_ERROR;

		Status added = addEntry(parent, name, static_cast<uint32_t>(nameBytes), number,
								cock::abi::ext2::DIRENT_TYPE_REGULAR);
		if (added != Status::OK) {
			freeInode(number);
			return added;
		}
		node = fresh;
		return Status::OK;
	}

	static Status create(const char *path, Node &node) {
		if (!begin()) return Status::IO_ERROR;
		return end(createFile(path, node));
	}

	static Status truncate(Node &node) {
		if (node.size >= MAX_FILE_BYTES) return Status::UNSUPPORTED;
		if (!begin()) return Status::IO_ERROR;
		return end(emptyNode(node));
	}

	static Status unlinkFile(const char *path) {
		Node parent = {};
		char name[MAX_NAME_BYTES + 1];
		size_t nameBytes = 0;
		if (!parentAndName(path, parent, name, nameBytes)) return Status::NOT_FOUND;

		Where where = {};
		if (!locateEntry(parent, name, where)) return Status::NOT_FOUND;

		Node node = {};
		if (!loadInode(where.inode, node)) return Status::IO_ERROR;
		if (node.type != FileType::REGULAR) return Status::UNSUPPORTED;
		if (node.size >= MAX_FILE_BYTES) return Status::UNSUPPORTED;

		Status removed = removeEntry(where);
		if (removed != Status::OK) return removed;

		if (node.links > cock::abi::ext2::NEW_FILE_LINKS) {
			node.links--;
			return storeInode(node) ? Status::OK : Status::IO_ERROR;
		}

		node.links = NO_LINKS;
		Status emptied = emptyNode(node, true);
		if (emptied != Status::OK) return emptied;
		return freeInode(node.inode) ? Status::OK : Status::IO_ERROR;
	}

	static Status removeEntry(const Where &where) {
		if (!loadDirectoryBlock(where.physical)) return Status::IO_ERROR;
		if (where.hasPrevious) {
			uint8_t *record =
				directoryBuffer + where.previous + cock::abi::ext2::DIRENT_RECORD_OFFSET;
			cock::abi::ext2::detail::writeLe16(
				record, cock::abi::ext2::detail::readLe16(record) + where.recordBytes);
		} else {
			cock::abi::ext2::detail::writeLe32(directoryBuffer + where.offset, 0);
		}
		return storeDirectoryBlock(where.physical) ? Status::OK : Status::IO_ERROR;
	}

	static bool isReservedName(const char *name) {
		return strcmp(name, CURRENT_NAME) == 0 || strcmp(name, PARENT_NAME) == 0;
	}

	static uint8_t entryType(FileType type) {
		if (type == FileType::DIRECTORY) return cock::abi::ext2::DIRENT_TYPE_DIRECTORY;
		if (type == FileType::SYMLINK) return cock::abi::ext2::DIRENT_TYPE_SYMLINK;
		return cock::abi::ext2::DIRENT_TYPE_REGULAR;
	}

	static Status makeDirectoryNode(const char *path) {
		Node parent = {};
		char name[MAX_NAME_BYTES + 1];
		size_t nameBytes = 0;
		if (!parentAndName(path, parent, name, nameBytes)) return Status::NOT_FOUND;
		if (isReservedName(name)) return Status::BAD_REQUEST;

		uint32_t existing = 0;
		if (findEntry(parent, name, existing)) return Status::EXISTS;

		uint32_t group = (parent.inode - 1) / sb.inodesPerGroup;
		uint32_t number = 0;
		Status taken = allocateInode(group, number);
		if (taken != Status::OK) return taken;

		uint32_t block = 0;
		taken = allocateBlock(group, false, block);
		if (taken != Status::OK) {
			freeInode(number);
			return taken;
		}

		cock::abi::ext2::initDirectoryBlock(directoryBuffer, sb.blockBytes, number, parent.inode,
											sb.fileType);
		if (!storeDirectoryBlock(block)) return Status::IO_ERROR;

		Node fresh = {};
		fresh.inode = number;
		fresh.type = FileType::DIRECTORY;
		fresh.size = sb.blockBytes;
		fresh.links = cock::abi::ext2::NEW_DIRECTORY_LINKS;
		fresh.sectors = sb.sectorsPerBlock();
		fresh.blocks[0] = block;
		if (!storeInode(fresh, cock::abi::ext2::MODE_NEW_DIRECTORY)) return Status::IO_ERROR;

		Status added = addEntry(parent, name, static_cast<uint32_t>(nameBytes), number,
								cock::abi::ext2::DIRENT_TYPE_DIRECTORY);
		if (added != Status::OK) {
			freeBlock(block);
			freeInode(number);
			return added;
		}

		parent.links++;
		if (!storeInode(parent) ||
			!adjustGroup((number - 1) / sb.inodesPerGroup, 0, 0, 1))
			return Status::IO_ERROR;
		return Status::OK;
	}

	static Status makeDirectory(const char *path) {
		if (!begin()) return Status::IO_ERROR;
		return end(makeDirectoryNode(path));
	}

	static Status removeDirectoryNode(const char *path) {
		Node parent = {};
		char name[MAX_NAME_BYTES + 1];
		size_t nameBytes = 0;
		if (!parentAndName(path, parent, name, nameBytes)) return Status::NOT_FOUND;

		Where where = {};
		if (!locateEntry(parent, name, where)) return Status::NOT_FOUND;

		Node node = {};
		if (!loadInode(where.inode, node)) return Status::IO_ERROR;
		if (node.type != FileType::DIRECTORY || node.inode == cock::abi::ext2::ROOT_INODE)
			return Status::UNSUPPORTED;

		uint32_t offset = 0;
		DirItem item = {};
		while (nextEntry(node, offset, item))
			if (!(item.nameBytes == cock::abi::ext2::CURRENT_ENTRY_BYTES &&
				  memcmp(item.name, CURRENT_NAME, item.nameBytes) == 0) &&
				!(item.nameBytes == cock::abi::ext2::PARENT_ENTRY_BYTES &&
				  memcmp(item.name, PARENT_NAME, item.nameBytes) == 0))
				return Status::NOT_EMPTY;

		if (!locateEntry(parent, name, where)) return Status::IO_ERROR;
		Status removed = removeEntry(where);
		if (removed != Status::OK) return removed;

		if (parent.links > 0) parent.links--;
		if (!storeInode(parent)) return Status::IO_ERROR;

		node.links = NO_LINKS;
		Status emptied = emptyNode(node, true);
		if (emptied != Status::OK) return emptied;
		if (!freeInode(node.inode) ||
			!adjustGroup((node.inode - 1) / sb.inodesPerGroup, 0, 0, -1))
			return Status::IO_ERROR;
		return Status::OK;
	}

	static Status removeDirectory(const char *path) {
		if (!begin()) return Status::IO_ERROR;
		return end(removeDirectoryNode(path));
	}

	static bool isAncestor(uint32_t directory, uint32_t start) {
		uint32_t current = start;
		for (uint32_t depth = 0; depth < MAX_DEPTH; depth++) {
			if (current == directory) return true;
			if (current == cock::abi::ext2::ROOT_INODE) return false;

			Node node = {};
			uint32_t up = 0;
			if (!loadInode(current, node) || !findEntry(node, PARENT_NAME, up)) return true;
			current = up;
		}
		return true;
	}

	static Status renameNode(const char *from, const char *to) {
		Node oldParent = {};
		char oldName[MAX_NAME_BYTES + 1];
		size_t oldBytes = 0;
		if (!parentAndName(from, oldParent, oldName, oldBytes)) return Status::NOT_FOUND;

		Where where = {};
		if (!locateEntry(oldParent, oldName, where)) return Status::NOT_FOUND;
		Node node = {};
		if (!loadInode(where.inode, node)) return Status::IO_ERROR;

		Node newParent = {};
		char newName[MAX_NAME_BYTES + 1];
		size_t newBytes = 0;
		if (!parentAndName(to, newParent, newName, newBytes)) return Status::NOT_FOUND;
		if (isReservedName(newName) || isReservedName(oldName)) return Status::BAD_REQUEST;
		if (node.type == FileType::DIRECTORY && isAncestor(node.inode, newParent.inode))
			return Status::BAD_REQUEST;

		uint32_t existing = 0;
		if (findEntry(newParent, newName, existing)) return Status::EXISTS;

		Status added = addEntry(newParent, newName, static_cast<uint32_t>(newBytes), node.inode,
								entryType(node.type));
		if (added != Status::OK) return added;

		if (!locateEntry(oldParent, oldName, where)) return Status::IO_ERROR;
		Status removed = removeEntry(where);
		if (removed != Status::OK) return removed;
		if (node.type != FileType::DIRECTORY) return Status::OK;

		uint32_t physical = 0;
		if (!physicalBlock(node, 0, physical) || physical == 0 || !loadDirectoryBlock(physical))
			return Status::IO_ERROR;
		uint32_t dotBytes = cock::abi::ext2::direntBytes(cock::abi::ext2::CURRENT_ENTRY_BYTES);
		cock::abi::ext2::detail::writeLe32(directoryBuffer + dotBytes, newParent.inode);
		if (!storeDirectoryBlock(physical)) return Status::IO_ERROR;

		if (oldParent.inode == newParent.inode) return Status::OK;
		Node before = {};
		Node after = {};
		if (!loadInode(oldParent.inode, before) || !loadInode(newParent.inode, after))
			return Status::IO_ERROR;
		if (before.links > 0) before.links--;
		after.links++;
		return storeInode(before) && storeInode(after) ? Status::OK : Status::IO_ERROR;
	}

	static Status rename(const char *from, const char *to) {
		if (!begin()) return Status::IO_ERROR;
		return end(renameNode(from, to));
	}

	static Status unlink(const char *path) {
		if (!begin()) return Status::IO_ERROR;
		return end(unlinkFile(path));
	}

	static Status writeBlocks(Node &node, const uint8_t *source, uint32_t offset,
							  uint32_t count, uint32_t &written) {
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
		if ((node.size != originalSize || node.sectors != originalSectors) &&
			!storeInode(node))
			result = Status::IO_ERROR;
		if (done > 0 && result == Status::NO_SPACE) result = Status::OK;
		return result;
	}

	static Status write(Handle &handle, const uint8_t *source, uint32_t offset,
						uint32_t count, uint32_t &written) {
		if (handle.node.size >= MAX_FILE_BYTES) return Status::UNSUPPORTED;
		if (!begin()) return Status::IO_ERROR;
		return end(writeBlocks(handle.node, source, offset, count, written));
	}

	static void report(cock::abi::ext2::Error error, uint32_t candidate) {
		using cock::abi::ext2::Error;
		switch (error) {
		case Error::BAD_MAGIC:
			printf("ext: device %u has no ext2 superblock\n", candidate);
			return;
		case Error::BAD_BLOCK_SIZE:
			printf("ext: device %u: block size is not supported\n", candidate);
			return;
		case Error::BAD_INODE_SIZE:
			printf("ext: device %u: inode size is not supported\n", candidate);
			return;
		case Error::BAD_GEOMETRY:
			printf("ext: device %u: the superblock does not fit the device\n", candidate);
			return;
		case Error::UNSUPPORTED_FEATURES:
			for (uint32_t bit = 1; bit != 0; bit <<= 1)
				if ((sb.unsupported & bit) != 0)
					printf("ext: device %u: unsupported feature %s\n", candidate,
						   cock::abi::ext2::featureName(bit));
			return;
		default:
			return;
		}
	}

	static bool mount(uint32_t candidate) {
		if (!disk.connect() || candidate >= disk.deviceCount()) {
			printf("ext: device %u does not exist\n", candidate);
			return false;
		}

		cock::user::block::DeviceInfo info = {};
		if (!disk.info(candidate, info) ||
			!disk.read(candidate, cock::abi::ext2::SUPERBLOCK_SECTOR,
					   cock::abi::ext2::SUPERBLOCK_SECTORS, superblockBuffer))
			return false;

		device = candidate;
		readOnly = info.kind == cock::abi::block::DeviceKind::DISK;
		clockReady = clock.init();
		cock::abi::ext2::Error error =
			cock::abi::ext2::parseSuperblock(superblockBuffer, info.sectorCount, sb);
		if (error != cock::abi::ext2::Error::NONE) {
			report(error, candidate);
			return false;
		}

		Node root = {};
		if (!loadInode(cock::abi::ext2::ROOT_INODE, root) || root.type != FileType::DIRECTORY) {
			printf("ext: device %u: the root directory is damaged\n", candidate);
			return false;
		}
		if (!writable()) printf("ext: device %u is mounted read only\n", candidate);
		return true;
	}
};

using Server = cock::user::FileServer<Ext>;

} // namespace

extern "C" void _start() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) break;
		Server::defer(message);
	}

	Ext::disk.onForeign = Server::defer;
	if (!Ext::mount(message.payload.args.arg1)) cock::user::exit_process(EXIT_FAILED_STATUS);
	printf("ext: device %u, %u blocks of %u bytes, %u inodes\n", Ext::device,
		   Ext::sb.blocksCount, Ext::sb.blockBytes, Ext::sb.inodesCount);

	Server::run();
}
