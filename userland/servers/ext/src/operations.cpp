#include <ext/alloc.hpp>
#include <ext/blockmap.hpp>
#include <ext/directory.hpp>
#include <ext/file.hpp>
#include <ext/inode.hpp>
#include <ext/lookup.hpp>
#include <ext/operations.hpp>
#include <ext/state.hpp>
#include <ext/volume.hpp>
#include <string.h>

using cock::abi::fs::FileType;
using cock::abi::fs::Status;

namespace cock::ext {

namespace {

constexpr uint32_t NO_LINKS = 0;

bool isReservedName(const char *name) {
	return strcmp(name, CURRENT_NAME) == 0 || strcmp(name, PARENT_NAME) == 0;
}

uint8_t entryType(FileType type) {
	if (type == FileType::DIRECTORY) return cock::abi::ext2::DIRENT_TYPE_DIRECTORY;
	if (type == FileType::SYMLINK) return cock::abi::ext2::DIRENT_TYPE_SYMLINK;
	return cock::abi::ext2::DIRENT_TYPE_REGULAR;
}

Status createEntry(const char *path, Node &node) {
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

Status unlinkEntry(const char *path) {
	Node parent = {};
	char name[MAX_NAME_BYTES + 1];
	size_t nameBytes = 0;
	if (!parentAndName(path, parent, name, nameBytes)) return Status::NOT_FOUND;

	Where where = {};
	if (!locateEntry(parent, name, where)) return Status::NOT_FOUND;

	Node node = {};
	if (!loadInode(where.inode, node)) return Status::IO_ERROR;
	if (node.type != FileType::REGULAR) return Status::UNSUPPORTED;
	if (node.size >= cock::abi::ext2::MAX_FILE_BYTES) return Status::UNSUPPORTED;

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

Status makeDirectoryNode(const char *path) {
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
	if (!storeInode(parent) || !adjustGroup((number - 1) / sb.inodesPerGroup, 0, 0, 1))
		return Status::IO_ERROR;
	return Status::OK;
}

Status removeDirectoryNode(const char *path) {
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

	if (!directoryIsEmpty(node)) return Status::NOT_EMPTY;

	if (!locateEntry(parent, name, where)) return Status::IO_ERROR;
	Status removed = removeEntry(where);
	if (removed != Status::OK) return removed;

	if (parent.links > 0) parent.links--;
	if (!storeInode(parent)) return Status::IO_ERROR;

	node.links = NO_LINKS;
	Status emptied = emptyNode(node, true);
	if (emptied != Status::OK) return emptied;
	if (!freeInode(node.inode) || !adjustGroup((node.inode - 1) / sb.inodesPerGroup, 0, 0, -1))
		return Status::IO_ERROR;
	return Status::OK;
}

Status renameNode(const char *from, const char *to) {
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
	if (findEntry(newParent, newName, existing)) {
		if (existing == node.inode) return Status::OK;

		Node victim = {};
		if (!loadInode(existing, victim)) return Status::IO_ERROR;
		bool sourceIsDirectory = node.type == FileType::DIRECTORY;
		bool victimIsDirectory = victim.type == FileType::DIRECTORY;
		if (sourceIsDirectory != victimIsDirectory ||
			(!sourceIsDirectory && victim.type != FileType::REGULAR))
			return Status::EXISTS;
		if (victimIsDirectory && !directoryIsEmpty(victim)) return Status::NOT_EMPTY;

		Status gone = victimIsDirectory ? removeDirectoryNode(to) : unlinkEntry(to);
		if (gone != Status::OK) return gone;

		if (!parentAndName(from, oldParent, oldName, oldBytes) ||
			!parentAndName(to, newParent, newName, newBytes))
			return Status::IO_ERROR;
	}

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

Status linkNode(const char *existing, const char *name) {
	Node parent = {};
	char leaf[MAX_NAME_BYTES + 1];
	size_t leafBytes = 0;
	if (!parentAndName(existing, parent, leaf, leafBytes)) return Status::NOT_FOUND;

	uint32_t inode = 0;
	if (!findEntry(parent, leaf, inode)) return Status::NOT_FOUND;
	Node node = {};
	if (!loadInode(inode, node)) return Status::IO_ERROR;
	if (node.type != FileType::REGULAR) return Status::UNSUPPORTED;
	if (node.links >= cock::abi::ext2::MAX_LINKS) return Status::NO_SPACE;

	Node newParent = {};
	char newName[MAX_NAME_BYTES + 1];
	size_t newBytes = 0;
	if (!parentAndName(name, newParent, newName, newBytes)) return Status::NOT_FOUND;
	if (isReservedName(newName)) return Status::BAD_REQUEST;

	uint32_t taken = 0;
	if (findEntry(newParent, newName, taken)) return Status::EXISTS;

	node.links++;
	if (!storeInode(node)) return Status::IO_ERROR;

	Status added = addEntry(newParent, newName, static_cast<uint32_t>(newBytes), node.inode,
							entryType(node.type));
	if (added == Status::OK) return Status::OK;

	node.links--;
	storeInode(node);
	return added;
}

} // namespace

Status createFile(const char *path, Node &node) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(createEntry(path, node));
}

Status truncateFile(Node &node) {
	if (node.size >= cock::abi::ext2::MAX_FILE_BYTES) return Status::UNSUPPORTED;
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(emptyNode(node));
}

Status unlinkFile(const char *path) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(unlinkEntry(path));
}

Status makeDirectory(const char *path) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(makeDirectoryNode(path));
}

Status removeDirectory(const char *path) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(removeDirectoryNode(path));
}

Status renameFile(const char *from, const char *to) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(renameNode(from, to));
}

Status linkFile(const char *existing, const char *name) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(linkNode(existing, name));
}

Status writeFile(Handle &handle, const uint8_t *source, uint32_t offset, uint32_t count,
				 uint32_t &written) {
	if (handle.node.size >= cock::abi::ext2::MAX_FILE_BYTES) return Status::UNSUPPORTED;
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(writeBlocks(handle.node, source, offset, count, written));
}

} // namespace cock::ext
