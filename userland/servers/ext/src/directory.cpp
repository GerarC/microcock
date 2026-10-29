#include <ext/blockmap.hpp>
#include <ext/directory.hpp>
#include <ext/inode.hpp>
#include <ext/volume.hpp>
#include <string.h>

using cock::abi::ext2::DirItem;
using cock::abi::fs::FileType;
using cock::abi::fs::Status;

namespace cock::ext {

bool loadDirectoryBlock(uint32_t physical) {
	if (physical == directoryLoaded) return true;
	directoryLoaded = NOTHING_LOADED;
	if (!disk.read(device, physical * sb.sectorsPerBlock(), sb.sectorsPerBlock(),
				   directoryBuffer))
		return false;
	directoryLoaded = physical;
	return true;
}

bool storeDirectoryBlock(uint32_t physical) {
	if (disk.write(device, physical * sb.sectorsPerBlock(), sb.sectorsPerBlock(),
				   directoryBuffer)) {
		directoryLoaded = physical;
		return true;
	}
	directoryLoaded = NOTHING_LOADED;
	return false;
}

bool nextEntry(const Node &directory, uint32_t &offset, DirItem &item) {
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

		if (!cock::abi::ext2::parseDirItem(directoryBuffer, sb.blockBytes, within, sb.fileType,
										   item)) {
			offset = nextBlock;
			continue;
		}
		offset += item.recordBytes;
		if (item.inode != 0 && item.nameBytes != 0) return true;
	}
}

bool findEntry(const Node &directory, const char *name, uint32_t &inode) {
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

bool locateEntry(const Node &directory, const char *name, Where &where) {
	size_t length = strlen(name);
	for (uint32_t fileBlock = 0; fileBlock * sb.blockBytes < directory.size; fileBlock++) {
		uint32_t physical = 0;
		if (!physicalBlock(directory, fileBlock, physical)) return false;
		if (physical == 0 || !loadDirectoryBlock(physical)) continue;

		uint32_t offset = 0;
		uint32_t previous = 0;
		bool hasPrevious = false;
		DirItem item = {};
		while (cock::abi::ext2::parseDirItem(directoryBuffer, sb.blockBytes, offset, sb.fileType,
											 item)) {
			if (item.inode != 0 && item.nameBytes == length &&
				memcmp(item.name, name, length) == 0) {
				where = {physical, offset, previous, hasPrevious, item.inode, item.recordBytes};
				return true;
			}
			previous = offset;
			hasPrevious = true;
			offset += item.recordBytes;
		}
	}
	return false;
}

Status addEntry(Node &directory, const char *name, uint32_t nameBytes, uint32_t inode,
				uint8_t type) {
	uint32_t need = cock::abi::ext2::direntBytes(nameBytes);
	uint32_t blocks = directory.size / sb.blockBytes;
	for (uint32_t fileBlock = 0; fileBlock < blocks; fileBlock++) {
		uint32_t physical = 0;
		if (!physicalBlock(directory, fileBlock, physical)) return Status::IO_ERROR;
		if (physical == 0) continue;
		if (!loadDirectoryBlock(physical)) return Status::IO_ERROR;

		uint32_t offset = 0;
		DirItem item = {};
		while (cock::abi::ext2::parseDirItem(directoryBuffer, sb.blockBytes, offset, sb.fileType,
											 item)) {
			uint32_t used = item.inode == 0 ? 0 : cock::abi::ext2::direntBytes(item.nameBytes);
			if (item.recordBytes - used >= need) {
				if (used != 0)
					cock::abi::ext2::detail::writeLe16(
						directoryBuffer + offset + cock::abi::ext2::DIRENT_RECORD_OFFSET, used);
				cock::abi::ext2::encodeDirItem(directoryBuffer + offset + used, inode,
											   item.recordBytes - used, name, nameBytes,
											   sb.fileType, type);
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

Status removeEntry(const Where &where) {
	if (!loadDirectoryBlock(where.physical)) return Status::IO_ERROR;
	if (where.hasPrevious) {
		uint8_t *record = directoryBuffer + where.previous + cock::abi::ext2::DIRENT_RECORD_OFFSET;
		cock::abi::ext2::detail::writeLe16(
			record, cock::abi::ext2::detail::readLe16(record) + where.recordBytes);
	} else {
		cock::abi::ext2::detail::writeLe32(directoryBuffer + where.offset, 0);
	}
	return storeDirectoryBlock(where.physical) ? Status::OK : Status::IO_ERROR;
}

bool directoryIsEmpty(const Node &node) {
	uint32_t offset = 0;
	DirItem item = {};
	while (nextEntry(node, offset, item))
		if (!(item.nameBytes == cock::abi::ext2::CURRENT_ENTRY_BYTES &&
			  memcmp(item.name, CURRENT_NAME, item.nameBytes) == 0) &&
			!(item.nameBytes == cock::abi::ext2::PARENT_ENTRY_BYTES &&
			  memcmp(item.name, PARENT_NAME, item.nameBytes) == 0))
			return false;
	return true;
}

bool listEntry(const Node &directory, uint32_t wanted, char *name, size_t capacity,
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

} // namespace cock::ext
