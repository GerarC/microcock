#include <fat/directory.hpp>
#include <fat/entries.hpp>
#include <fat/file.hpp>
#include <fat/fsinfo.hpp>
#include <fat/lookup.hpp>
#include <fat/operations.hpp>
#include <fat/stamp.hpp>
#include <fat/state.hpp>
#include <fat/table.hpp>
#include <fat/volume.hpp>
#include <string.h>

using cock::abi::fs::FileType;
using cock::abi::fs::Status;

namespace cock::fat {

namespace {

Status createEntry(const char *path, Node &node) {
	Node parent = {};
	char name[cock::abi::fs::MAX_PATH_BYTES];
	size_t nameBytes = 0;
	Status located = parentAndName(path, parent, name, nameBytes);
	if (located != Status::OK) return located;

	Found existing = {};
	if (findInDirectory(parent.cluster, name, existing)) return Status::EXISTS;

	uint8_t entry[cock::abi::fat32::ENTRY_BYTES];
	buildTemplate(entry, cock::abi::fat32::ATTRIBUTE_ARCHIVE, NO_CLUSTER);
	Spot where = {};
	Status added = placeEntry(parent.cluster, name, nameBytes, entry, where);
	flushFsInfo();
	if (added != Status::OK) return added;

	node = {NO_CLUSTER, NO_BYTES, FileType::REGULAR, where};
	return Status::OK;
}

Status makeDirectoryEntry(const char *path) {
	Node parent = {};
	char name[cock::abi::fs::MAX_PATH_BYTES];
	size_t nameBytes = 0;
	Status located = parentAndName(path, parent, name, nameBytes);
	if (located != Status::OK) return located;

	Found existing = {};
	if (findInDirectory(parent.cluster, name, existing)) return Status::EXISTS;

	uint32_t cluster = NO_CLUSTER;
	Status allocated = allocateCluster(NO_CLUSTER, cluster);
	if (allocated != Status::OK) {
		flushFsInfo();
		return allocated;
	}

	uint8_t sector[cock::abi::fat32::SECTOR_BYTES] = {};
	uint8_t dotName[cock::abi::fat32::ENTRY_SHORT_NAME_BYTES];
	uint16_t date = 0;
	uint16_t time = 0;
	currentStamp(date, time);
	cock::abi::fat32::dotShortName(false, dotName);
	cock::abi::fat32::encodeEntry(sector, dotName, 0, cock::abi::fat32::ATTRIBUTE_DIRECTORY,
								  cluster, NO_BYTES, date, time);
	cock::abi::fat32::dotShortName(true, dotName);
	cock::abi::fat32::encodeEntry(
		sector + cock::abi::fat32::ENTRY_BYTES, dotName, 0,
		cock::abi::fat32::ATTRIBUTE_DIRECTORY,
		parent.cluster == bpb.rootCluster ? NO_CLUSTER : parent.cluster, NO_BYTES, date, time);

	Status placed = Status::IO_ERROR;
	if (zeroCluster(cluster) &&
		disk.write(device, bpb.clusterSector(cluster), SINGLE_SECTOR, sector)) {
		uint8_t entry[cock::abi::fat32::ENTRY_BYTES];
		buildTemplate(entry, cock::abi::fat32::ATTRIBUTE_DIRECTORY, cluster);
		Spot where = {};
		placed = placeEntry(parent.cluster, name, nameBytes, entry, where);
	}
	if (placed != Status::OK) freeChain(cluster);
	flushFsInfo();
	return placed;
}

Status removeDirectoryEntry(const char *path) {
	Node node = {};
	Found leaf = {};
	if (!walk(path, node, leaf) || node.type != FileType::DIRECTORY ||
		node.cluster == bpb.rootCluster)
		return Status::NOT_FOUND;

	bool empty = false;
	Status scanned = isEmptyDirectory(node.cluster, empty);
	if (scanned != Status::OK) return scanned;
	if (!empty) return Status::NOT_EMPTY;

	Status deleted = deleteEntries(leaf);
	if (deleted != Status::OK) return deleted;
	bool freed = freeChain(node.cluster);
	flushFsInfo();
	return freed ? Status::OK : Status::IO_ERROR;
}

Status checkReplace(const Node &source, const Found &existing) {
	bool sourceIsDirectory = source.type == FileType::DIRECTORY;
	if (existing.entry.isDirectory() != sourceIsDirectory) return Status::EXISTS;
	if (!sourceIsDirectory) return Status::OK;
	if (!bpb.isDataCluster(existing.entry.cluster)) return Status::IO_ERROR;

	bool empty = false;
	Status scanned = isEmptyDirectory(existing.entry.cluster, empty);
	if (scanned != Status::OK) return scanned;
	return empty ? Status::OK : Status::NOT_EMPTY;
}

Status removeFound(const Found &existing) {
	Status deleted = deleteEntries(existing);
	if (deleted != Status::OK) return deleted;
	bool freed = !bpb.isDataCluster(existing.entry.cluster) || freeChain(existing.entry.cluster);
	flushFsInfo();
	return freed ? Status::OK : Status::IO_ERROR;
}

Status renameEntry(const char *from, const char *to) {
	Node source = {};
	Found leaf = {};
	if (!walk(from, source, leaf)) return Status::NOT_FOUND;
	if (source.cluster == bpb.rootCluster) return Status::BAD_REQUEST;

	char parentPath[cock::abi::fs::MAX_PATH_BYTES];
	const char *target = nullptr;
	size_t targetBytes = 0;
	if (!cock::abi::fs::splitPath(to, parentPath, sizeof(parentPath), target, targetBytes))
		return Status::BAD_REQUEST;
	if (source.type == FileType::DIRECTORY && isInside(from, parentPath))
		return Status::BAD_REQUEST;

	char name[cock::abi::fs::MAX_PATH_BYTES];
	memcpy(name, target, targetBytes);
	name[targetBytes] = '\0';
	Node parent = {};
	if (!resolve(parentPath, parent) || parent.type != FileType::DIRECTORY)
		return Status::NOT_FOUND;

	Found existing = {};
	bool exists = findInDirectory(parent.cluster, name, existing);
	bool caseOnly = exists && sameSpot(existing.shortEntry, leaf.shortEntry);
	bool replaces = exists && !caseOnly;
	if (replaces) {
		Status allowed = checkReplace(source, existing);
		if (allowed != Status::OK) return allowed;
	}

	uint32_t sector = spotSector(leaf.shortEntry);
	if (!loadDirectory(sector)) return Status::IO_ERROR;
	uint8_t entry[cock::abi::fat32::ENTRY_BYTES];
	memcpy(entry,
		   directoryBuffer + size_t(leaf.shortEntry.entry) * cock::abi::fat32::ENTRY_BYTES,
		   sizeof(entry));

	if (replaces) {
		Status gone = removeFound(existing);
		if (gone != Status::OK) return gone;
	}

	Spot where = {};
	Status placed = Status::OK;
	if (caseOnly) {
		placed = deleteEntries(leaf);
		if (placed == Status::OK)
			placed = placeEntry(parent.cluster, name, targetBytes, entry, where);
	} else {
		placed = placeEntry(parent.cluster, name, targetBytes, entry, where);
		if (placed == Status::OK) placed = deleteEntries(leaf);
	}
	if (placed == Status::OK && source.type == FileType::DIRECTORY) {
		Spot dotDot = {source.cluster, 0, 1};
		uint32_t dotSector = spotSector(dotDot);
		if (!loadDirectory(dotSector)) {
			placed = Status::IO_ERROR;
		} else {
			cock::abi::fat32::patchEntryLocation(
				directoryBuffer + size_t(dotDot.entry) * cock::abi::fat32::ENTRY_BYTES,
				parent.cluster == bpb.rootCluster ? NO_CLUSTER : parent.cluster, NO_BYTES);
			if (!storeDirectory(dotSector)) placed = Status::IO_ERROR;
		}
	}
	flushFsInfo();
	return placed;
}

Status unlinkEntry(const char *path) {
	Node node = {};
	Found leaf = {};
	if (!walk(path, node, leaf)) return Status::NOT_FOUND;
	if (node.type != FileType::REGULAR) return Status::UNSUPPORTED;

	Status deleted = deleteEntries(leaf);
	if (deleted != Status::OK) return deleted;

	bool freed = node.cluster == NO_CLUSTER || freeChain(node.cluster);
	flushFsInfo();
	return freed ? Status::OK : Status::IO_ERROR;
}

} // namespace

Status createFile(const char *path, Node &node) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(createEntry(path, node));
}

Status makeDirectory(const char *path) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(makeDirectoryEntry(path));
}

Status removeDirectory(const char *path) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(removeDirectoryEntry(path));
}

Status renameFile(const char *from, const char *to) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(renameEntry(from, to));
}

Status truncateFile(Node &node) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(truncateNode(node));
}

Status writeFile(Handle &handle, const uint8_t *source, uint32_t offset, uint32_t count,
				 uint32_t &written) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(writeBytes(handle, source, offset, count, written));
}

Status unlinkFile(const char *path) {
	if (!beginUpdate()) return Status::IO_ERROR;
	return endUpdate(unlinkEntry(path));
}

} // namespace cock::fat
