#include <cock/abi/block.hpp>
#include <cock/abi/ext2.hpp>
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/block.hpp>
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
constexpr char PATH_SEPARATOR = '/';
constexpr char CURRENT_NAME[] = ".";
constexpr char PARENT_NAME[] = "..";

struct Ext {
	struct Node {
		uint32_t inode;
		uint32_t size;
		FileType type;
		uint32_t blocks[cock::abi::ext2::INODE_BLOCK_SLOTS];
	};

	struct Handle {
		bool open;
		Node node;
	};

	inline static cock::user::block::Client disk;
	inline static uint32_t device = 0;
	inline static Superblock sb = {};
	inline static uint8_t superblockBuffer[cock::abi::ext2::SUPERBLOCK_BYTES];
	inline static uint8_t groupBuffer[SECTOR_BYTES];
	inline static uint32_t groupLoaded = NOTHING_LOADED;
	inline static uint8_t inodeBuffer[SECTOR_BYTES];
	inline static uint32_t inodeLoaded = NOTHING_LOADED;
	inline static uint8_t pointerBuffer[SECTOR_BYTES];
	inline static uint32_t pointerLoaded = NOTHING_LOADED;
	inline static uint8_t directoryBuffer[cock::abi::ext2::MAX_BLOCK_BYTES];
	inline static uint32_t directoryLoaded = NOTHING_LOADED;
	inline static uint8_t dataBuffer[SECTOR_BYTES];

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

	static bool loadInode(uint32_t number, Node &node) {
		if (number == 0 || number > sb.inodesCount) return false;

		uint32_t index = number - 1;
		uint32_t group = index / sb.inodesPerGroup;
		uint32_t within = index % sb.inodesPerGroup;
		if (group >= sb.groupCount) return false;

		uint32_t descriptor = group * static_cast<uint32_t>(cock::abi::ext2::GROUP_DESCRIPTOR_BYTES);
		if (!loadSector(sb.groupTableSector() + descriptor / SECTOR_BYTES, groupBuffer,
						groupLoaded))
			return false;
		uint32_t table = cock::abi::ext2::detail::readLe32(
			groupBuffer + descriptor % SECTOR_BYTES + cock::abi::ext2::GD_INODE_TABLE_OFFSET);

		uint32_t byte = within * sb.inodeBytes;
		if (!validBlock(table) || !validBlock(table + byte / sb.blockBytes)) return false;
		if (!loadSector(table * sb.sectorsPerBlock() + byte / SECTOR_BYTES, inodeBuffer,
						inodeLoaded))
			return false;

		Inode inode = cock::abi::ext2::parseInode(inodeBuffer + byte % SECTOR_BYTES);
		if ((inode.flags & (cock::abi::ext2::INODE_EXTENTS_FLAG |
							cock::abi::ext2::INODE_INLINE_DATA_FLAG)) != 0)
			return false;

		node.inode = number;
		node.size = inode.size;
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

			if (physical != directoryLoaded) {
				directoryLoaded = NOTHING_LOADED;
				if (!disk.read(device, physical * sb.sectorsPerBlock(), sb.sectorsPerBlock(),
							   directoryBuffer))
					return false;
				directoryLoaded = physical;
			}

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
