#include <fat/directory.hpp>
#include <fat/stamp.hpp>
#include <fat/table.hpp>
#include <fat/volume.hpp>
#include <string.h>

using cock::abi::fat32::DirectoryParser;
using cock::abi::fat32::DirEntry;
using cock::abi::fs::FileType;
using cock::abi::fs::Status;

namespace {

bool nextEntry(cock::fat::Cursor &cursor, DirectoryParser &parser, DirEntry &entry) {
	const uint8_t *raw = nullptr;
	cock::fat::Spot here = {};
	while (cock::fat::nextRaw(cursor, raw, here) == cock::fat::Walk::ENTRY) {
		DirectoryParser::Result result = parser.feed(raw, entry);
		if (result == DirectoryParser::Result::END) return false;
		if (result == DirectoryParser::Result::ENTRY) return true;
	}
	return false;
}

} // namespace

namespace cock::fat {

bool loadDirectory(uint32_t sector) {
	if (sector == directoryLoaded) return true;
	directoryLoaded = NOTHING_LOADED;
	if (!disk.read(device, sector, SINGLE_SECTOR, directoryBuffer)) return false;
	directoryLoaded = sector;
	return true;
}

bool storeDirectory(uint32_t sector) {
	if (disk.write(device, sector, SINGLE_SECTOR, directoryBuffer)) return true;
	directoryLoaded = NOTHING_LOADED;
	return false;
}

Walk nextRaw(Cursor &cursor, const uint8_t *&raw, Spot &here) {
	if (cursor.entry == cock::abi::fat32::ENTRIES_PER_SECTOR) {
		cursor.entry = 0;
		cursor.sector++;
	}
	if (cursor.sector == bpb.sectorsPerCluster) {
		uint32_t next = NO_CLUSTER;
		Link link = nextCluster(cursor.cluster, next);
		if (link == Link::END) return Walk::END;
		if (link != Link::NEXT || ++cursor.steps > bpb.clusterCount) return Walk::FAILED;
		cursor.cluster = next;
		cursor.sector = 0;
	}

	uint32_t sector = bpb.clusterSector(cursor.cluster) + cursor.sector;
	if (!loadDirectory(sector)) return Walk::FAILED;

	raw = directoryBuffer + size_t(cursor.entry) * cock::abi::fat32::ENTRY_BYTES;
	here = {cursor.cluster, cursor.sector, cursor.entry};
	cursor.entry++;
	return Walk::ENTRY;
}

bool stepSpot(Spot &spot) {
	spot.entry++;
	if (spot.entry < cock::abi::fat32::ENTRIES_PER_SECTOR) return true;
	spot.entry = 0;
	spot.sector++;
	if (spot.sector < bpb.sectorsPerCluster) return true;

	uint32_t next = NO_CLUSTER;
	if (nextCluster(spot.cluster, next) != Link::NEXT) return false;
	spot.cluster = next;
	spot.sector = 0;
	return true;
}

bool findInDirectory(uint32_t directory, const char *name, Found &found) {
	DirectoryParser parser;
	parser.reset();
	Cursor cursor = {directory, 0, 0, 0};
	Spot chainStart = {};
	bool chaining = false;
	const uint8_t *raw = nullptr;
	Spot here = {};
	while (nextRaw(cursor, raw, here) == Walk::ENTRY) {
		uint8_t first = raw[cock::abi::fat32::ENTRY_NAME_OFFSET];
		uint8_t attributes = raw[cock::abi::fat32::ENTRY_ATTRIBUTES_OFFSET];
		bool longPart = first != cock::abi::fat32::DIRECTORY_DELETED &&
						first != cock::abi::fat32::DIRECTORY_END &&
						(attributes & cock::abi::fat32::ATTRIBUTE_LONG_NAME_MASK) ==
							cock::abi::fat32::ATTRIBUTE_LONG_NAME;
		if (longPart && (first & cock::abi::fat32::LONG_LAST_FLAG) != 0) {
			chainStart = here;
			chaining = true;
		}

		DirectoryParser::Result result = parser.feed(raw, found.entry);
		if (result == DirectoryParser::Result::END) return false;
		if (result == DirectoryParser::Result::ENTRY) {
			if (cock::abi::fat32::namesEqual(found.entry.name, name)) {
				found.shortEntry = here;
				found.firstEntry = chaining ? chainStart : here;
				return true;
			}
			chaining = false;
		} else if (!longPart) {
			chaining = false;
		}
	}
	return false;
}

bool listEntry(const Node &directory, uint32_t wanted, char *name, size_t capacity,
			   FileType &type) {
	uint32_t seen = 0;
	DirectoryParser parser;
	parser.reset();
	Cursor cursor = {directory.cluster, 0, 0, 0};
	DirEntry entry = {};
	while (nextEntry(cursor, parser, entry)) {
		if (seen++ != wanted) continue;

		size_t length = strlen(entry.name);
		if (length + 1 > capacity) return false;
		memcpy(name, entry.name, length + 1);
		type = entry.isDirectory() ? FileType::DIRECTORY : FileType::REGULAR;
		return true;
	}
	return false;
}

Status writeEntryAt(const Spot &spot, const uint8_t *entry) {
	uint32_t sector = spotSector(spot);
	if (!loadDirectory(sector)) return Status::IO_ERROR;
	memcpy(directoryBuffer + size_t(spot.entry) * cock::abi::fat32::ENTRY_BYTES, entry,
		   cock::abi::fat32::ENTRY_BYTES);
	return storeDirectory(sector) ? Status::OK : Status::IO_ERROR;
}

Status deleteEntries(const Found &leaf) {
	Spot at = leaf.firstEntry;
	for (;;) {
		uint32_t sector = spotSector(at);
		if (!loadDirectory(sector)) return Status::IO_ERROR;
		directoryBuffer[size_t(at.entry) * cock::abi::fat32::ENTRY_BYTES +
						cock::abi::fat32::ENTRY_NAME_OFFSET] =
			cock::abi::fat32::DIRECTORY_DELETED;
		if (!storeDirectory(sector)) return Status::IO_ERROR;
		if (sameSpot(at, leaf.shortEntry)) return Status::OK;
		if (!stepSpot(at)) return Status::IO_ERROR;
	}
}

Status commitEntry(const Node &node) {
	uint32_t sector = spotSector(node.entry);
	if (!loadDirectory(sector)) return Status::IO_ERROR;

	uint8_t *raw = directoryBuffer + size_t(node.entry.entry) * cock::abi::fat32::ENTRY_BYTES;
	uint16_t date = 0;
	uint16_t time = 0;
	currentStamp(date, time);
	cock::abi::fat32::patchEntryLocation(raw, node.cluster, node.size);
	cock::abi::fat32::touchEntry(raw, date, time);
	return storeDirectory(sector) ? Status::OK : Status::IO_ERROR;
}

Status isEmptyDirectory(uint32_t directory, bool &empty) {
	empty = true;
	Cursor cursor = {directory, 0, 0, 0};
	const uint8_t *raw = nullptr;
	Spot here = {};
	for (;;) {
		Walk step = nextRaw(cursor, raw, here);
		if (step == Walk::FAILED) return Status::IO_ERROR;
		if (step == Walk::END) return Status::OK;

		uint8_t first = raw[cock::abi::fat32::ENTRY_NAME_OFFSET];
		if (first == cock::abi::fat32::DIRECTORY_END) return Status::OK;
		if (first == cock::abi::fat32::DIRECTORY_DELETED) continue;

		uint8_t attributes = raw[cock::abi::fat32::ENTRY_ATTRIBUTES_OFFSET];
		if ((attributes & cock::abi::fat32::ATTRIBUTE_LONG_NAME_MASK) ==
				cock::abi::fat32::ATTRIBUTE_LONG_NAME ||
			(attributes & cock::abi::fat32::ATTRIBUTE_VOLUME_LABEL) != 0)
			continue;
		if (first == uint8_t(cock::abi::fat32::DOT) &&
			(raw[cock::abi::fat32::DOT_SECOND_BYTE] == uint8_t(cock::abi::fat32::SPACE) ||
			 raw[cock::abi::fat32::DOT_SECOND_BYTE] == uint8_t(cock::abi::fat32::DOT)))
			continue;
		empty = false;
		return Status::OK;
	}
}

} // namespace cock::fat
