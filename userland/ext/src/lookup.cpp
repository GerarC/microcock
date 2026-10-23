#include <ext/directory.hpp>
#include <ext/file.hpp>
#include <ext/inode.hpp>
#include <ext/lookup.hpp>
#include <ext/volume.hpp>
#include <string.h>

using cock::abi::fs::FileType;
using cock::ext::Node;

namespace {

constexpr size_t MAX_LINK_BYTES = 255;
constexpr size_t WORK_BYTES = 512;
constexpr uint32_t MAX_LINKS = 8;
constexpr uint32_t MAX_DEPTH = 256;
constexpr char PATH_SEPARATOR = '/';

bool readLink(const Node &link, char *target, size_t capacity) {
	if (link.size == 0 || link.size >= capacity) return false;

	if (link.size < cock::abi::ext2::FAST_SYMLINK_BYTES)
		memcpy(target, link.blocks, link.size);
	else if (!cock::ext::readBytes(link, reinterpret_cast<uint8_t *>(target), 0, link.size))
		return false;
	target[link.size] = '\0';
	return true;
}

} // namespace

namespace cock::ext {

bool resolve(const char *path, Node &result) {
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

bool parentAndName(const char *path, Node &parent, char *name, size_t &nameBytes) {
	char parentPath[cock::abi::fs::MAX_PATH_BYTES];
	const char *leaf = nullptr;
	if (!cock::abi::fs::splitPath(path, parentPath, sizeof(parentPath), leaf, nameBytes) ||
		nameBytes > MAX_NAME_BYTES)
		return false;

	memcpy(name, leaf, nameBytes);
	name[nameBytes] = '\0';
	return resolve(parentPath, parent) && parent.type == FileType::DIRECTORY;
}

bool resolveLeaf(const char *path, Node &node) {
	Node parent = {};
	char name[MAX_NAME_BYTES + 1];
	size_t nameBytes = 0;
	if (!parentAndName(path, parent, name, nameBytes)) return false;

	uint32_t inode = 0;
	return findEntry(parent, name, inode) && loadInode(inode, node);
}

bool isAncestor(uint32_t directory, uint32_t start) {
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

} // namespace cock::ext
