#include <fat/directory.hpp>
#include <fat/lookup.hpp>
#include <fat/volume.hpp>
#include <string.h>

using cock::abi::fat32::DirEntry;
using cock::abi::fs::FileType;
using cock::abi::fs::Status;

namespace {

bool nextComponent(const char *&cursor, char *component, size_t capacity) {
	while (*cursor == cock::fat::PATH_SEPARATOR)
		cursor++;
	if (*cursor == '\0') return false;

	size_t length = 0;
	while (*cursor != '\0' && *cursor != cock::fat::PATH_SEPARATOR) {
		if (length + 1 >= capacity) return false;
		component[length++] = *cursor++;
	}
	component[length] = '\0';
	return true;
}

} // namespace

namespace cock::fat {

bool walk(const char *path, Node &node, Found &leaf) {
	node = {bpb.rootCluster, NO_BYTES, FileType::DIRECTORY, {}};
	while (*path != '\0') {
		while (*path == PATH_SEPARATOR)
			path++;
		if (*path == '\0') break;

		char component[cock::abi::fat32::MAX_NAME_BYTES];
		size_t length = 0;
		while (*path != '\0' && *path != PATH_SEPARATOR) {
			if (length + 1 >= sizeof(component)) return false;
			component[length++] = *path++;
		}
		component[length] = '\0';

		if (node.type != FileType::DIRECTORY) return false;
		if (!findInDirectory(node.cluster, component, leaf)) return false;

		const DirEntry &entry = leaf.entry;
		if (entry.isDirectory()) {
			if (!bpb.isDataCluster(entry.cluster)) return false;
			node = {entry.cluster, NO_BYTES, FileType::DIRECTORY, leaf.shortEntry};
		} else {
			if (entry.size != NO_BYTES && !bpb.isDataCluster(entry.cluster)) return false;
			node = {entry.cluster, entry.size, FileType::REGULAR, leaf.shortEntry};
		}
	}
	return true;
}

bool resolve(const char *path, Node &node) {
	Found leaf = {};
	return walk(path, node, leaf);
}

Status parentAndName(const char *path, Node &parent, char *name, size_t &nameBytes) {
	char parentPath[cock::abi::fs::MAX_PATH_BYTES];
	const char *leaf = nullptr;
	if (!cock::abi::fs::splitPath(path, parentPath, sizeof(parentPath), leaf, nameBytes))
		return Status::BAD_REQUEST;

	memcpy(name, leaf, nameBytes);
	name[nameBytes] = '\0';
	if (!resolve(parentPath, parent) || parent.type != FileType::DIRECTORY)
		return Status::NOT_FOUND;
	return Status::OK;
}

bool isInside(const char *ancestor, const char *path) {
	char first[cock::abi::fs::MAX_PATH_BYTES];
	char second[cock::abi::fs::MAX_PATH_BYTES];
	while (nextComponent(ancestor, first, sizeof(first))) {
		if (!nextComponent(path, second, sizeof(second)) ||
			!cock::abi::fat32::namesEqual(first, second))
			return false;
	}
	return true;
}

} // namespace cock::fat
