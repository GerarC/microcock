#include <fat/directory.hpp>
#include <fat/entries.hpp>
#include <fat/stamp.hpp>
#include <fat/table.hpp>
#include <fat/volume.hpp>
#include <string.h>

using cock::abi::fs::Status;
using cock::fat::Cursor;
using cock::fat::Spot;
using cock::fat::Walk;

namespace {

bool isFreeEntry(uint8_t first) {
	return first == cock::abi::fat32::DIRECTORY_END ||
		   first == cock::abi::fat32::DIRECTORY_DELETED;
}

Status findFreeRun(uint32_t directory, size_t needed, Spot &start) {
	Cursor cursor = {directory, 0, 0, 0};
	const uint8_t *raw = nullptr;
	Spot here = {};
	Spot runStart = {};
	size_t run = 0;
	for (;;) {
		Walk step = cock::fat::nextRaw(cursor, raw, here);
		if (step == Walk::FAILED) return Status::IO_ERROR;
		if (step == Walk::END) break;

		if (!isFreeEntry(raw[cock::abi::fat32::ENTRY_NAME_OFFSET])) {
			run = 0;
			continue;
		}
		if (run == 0) runStart = here;
		if (++run >= needed) {
			start = runStart;
			return Status::OK;
		}
	}

	uint32_t last = cursor.cluster;
	const size_t perCluster =
		size_t(cock::fat::bpb.sectorsPerCluster) * cock::abi::fat32::ENTRIES_PER_SECTOR;
	while (run < needed) {
		uint32_t added = cock::fat::NO_CLUSTER;
		Status allocated = cock::fat::allocateCluster(cock::fat::NO_CLUSTER, added);
		if (allocated != Status::OK) return allocated;
		if (!cock::fat::zeroCluster(added) || !cock::fat::setLink(last, added))
			return Status::IO_ERROR;
		if (run == 0) runStart = {added, 0, 0};
		run += perCluster;
		last = added;
	}
	start = runStart;
	return Status::OK;
}

Status shortNameTaken(uint32_t directory, const uint8_t *shortName, bool &taken) {
	taken = false;
	Cursor cursor = {directory, 0, 0, 0};
	const uint8_t *raw = nullptr;
	Spot here = {};
	for (;;) {
		Walk step = cock::fat::nextRaw(cursor, raw, here);
		if (step == Walk::FAILED) return Status::IO_ERROR;
		if (step == Walk::END) return Status::OK;

		uint8_t first = raw[cock::abi::fat32::ENTRY_NAME_OFFSET];
		if (first == cock::abi::fat32::DIRECTORY_END) return Status::OK;
		if (first == cock::abi::fat32::DIRECTORY_DELETED) continue;
		uint8_t attributes = raw[cock::abi::fat32::ENTRY_ATTRIBUTES_OFFSET];
		if ((attributes & cock::abi::fat32::ATTRIBUTE_LONG_NAME_MASK) ==
			cock::abi::fat32::ATTRIBUTE_LONG_NAME)
			continue;
		if (memcmp(raw, shortName, cock::abi::fat32::ENTRY_SHORT_NAME_BYTES) == 0) {
			taken = true;
			return Status::OK;
		}
	}
}

} // namespace

namespace cock::fat {

void buildTemplate(uint8_t *entry, uint8_t attributes, uint32_t cluster) {
	uint8_t blank[cock::abi::fat32::ENTRY_SHORT_NAME_BYTES];
	memset(blank, cock::abi::fat32::SPACE, sizeof(blank));
	uint16_t date = 0;
	uint16_t time = 0;
	currentStamp(date, time);
	cock::abi::fat32::encodeEntry(entry, blank, 0, attributes, cluster, NO_BYTES, date, time);
}

Status placeEntry(uint32_t directory, const char *name, size_t nameBytes,
				  const uint8_t *entryTemplate, Spot &where) {
	uint8_t shortName[cock::abi::fat32::ENTRY_SHORT_NAME_BYTES];
	uint8_t caseFlags = 0;
	cock::abi::fat32::NamePlan plan =
		cock::abi::fat32::planName(name, nameBytes, shortName, caseFlags);

	uint16_t units[cock::abi::fat32::MAX_NAME_CHARS];
	size_t unitCount = 0;
	size_t longEntries = 0;
	if (plan != cock::abi::fat32::NamePlan::SHORT) {
		if (!cock::abi::fat32::decodeUtf8Name(name, nameBytes, units,
											  cock::abi::fat32::MAX_NAME_CHARS, unitCount))
			return Status::BAD_REQUEST;
		longEntries = cock::abi::fat32::longEntryCount(unitCount);
	}

	bool taken = false;
	if (plan == cock::abi::fat32::NamePlan::LOSSY) {
		uint32_t tail = 1;
		for (;; tail++) {
			if (tail > cock::abi::fat32::MAX_ALIAS_TAIL) return Status::NO_SPACE;
			cock::abi::fat32::makeLossyAlias(name, nameBytes, tail, shortName);
			Status checked = shortNameTaken(directory, shortName, taken);
			if (checked != Status::OK) return checked;
			if (!taken) break;
		}
	} else {
		Status checked = shortNameTaken(directory, shortName, taken);
		if (checked != Status::OK) return checked;
		if (taken) return Status::EXISTS;
	}

	Spot start = {};
	Status found = findFreeRun(directory, longEntries + 1, start);
	if (found != Status::OK) return found;

	Spot shortSpot = start;
	for (size_t step = 0; step < longEntries; step++)
		if (!stepSpot(shortSpot)) return Status::IO_ERROR;

	uint8_t entry[cock::abi::fat32::ENTRY_BYTES];
	memcpy(entry, entryTemplate, sizeof(entry));
	memcpy(entry + cock::abi::fat32::ENTRY_NAME_OFFSET, shortName,
		   cock::abi::fat32::ENTRY_SHORT_NAME_BYTES);
	entry[cock::abi::fat32::ENTRY_CASE_OFFSET] = caseFlags;
	Status written = writeEntryAt(shortSpot, entry);
	if (written != Status::OK) return written;

	uint8_t checksum = cock::abi::fat32::shortNameChecksum(shortName);
	Spot at = start;
	for (size_t index = 0; index < longEntries; index++) {
		cock::abi::fat32::encodeLongEntry(entry, longEntries - index, index == 0, checksum, units,
										  unitCount);
		written = writeEntryAt(at, entry);
		if (written != Status::OK) return written;
		if (!stepSpot(at)) return Status::IO_ERROR;
	}
	where = shortSpot;
	return Status::OK;
}

} // namespace cock::fat
