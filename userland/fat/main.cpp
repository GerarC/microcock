#include <cock/abi/block.hpp>
#include <cock/abi/fat32.hpp>
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/abi/datetime.hpp>
#include <cock/user/block.hpp>
#include <cock/user/clock.hpp>
#include <cock/user/file_server.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using cock::abi::fat32::Bpb;
using cock::abi::fat32::DirectoryParser;
using cock::abi::fat32::DirEntry;
using cock::abi::fs::FileType;
using cock::abi::fs::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;
constexpr uint32_t NO_CLUSTER = 0;
constexpr uint32_t NOTHING_LOADED = 0xFFFFFFFF;
constexpr uint32_t SECTOR_ZERO = 0;
constexpr uint32_t SINGLE_SECTOR = 1;
constexpr uint32_t NO_BYTES = 0;
constexpr uint32_t MAX_FILE_SIZE = 0xFFFFFFFF;
constexpr uint32_t FIRST_FSINFO_SECTOR = 1;
constexpr char PATH_SEPARATOR = '/';

struct Cursor {
	uint32_t cluster;
	uint32_t sector;
	uint32_t entry;
	uint32_t steps;
};

struct Spot {
	uint32_t cluster;
	uint32_t sector;
	uint32_t entry;
};

struct Found {
	DirEntry entry;
	Spot shortEntry;
	Spot firstEntry;
};

enum class Link : uint32_t {
	NEXT,
	END,
	BAD,
};

enum class Walk : uint32_t {
	ENTRY,
	END,
	FAILED,
};

bool sameSpot(const Spot &first, const Spot &second) {
	return first.cluster == second.cluster && first.sector == second.sector &&
		   first.entry == second.entry;
}

struct Fat {
	struct Node {
		uint32_t cluster;
		uint32_t size;
		FileType type;
		Spot entry;
	};

	struct Handle {
		bool open;
		Node node;
		uint32_t cursorIndex;
		uint32_t cursorCluster;
	};

	static constexpr uint32_t MAX_FILE_BYTES = MAX_FILE_SIZE;

	inline static cock::user::block::Client disk;
	inline static uint32_t device = 0;
	inline static bool readOnly = true;
	inline static Bpb bpb = {};
	inline static uint8_t fatBuffer[cock::abi::fat32::SECTOR_BYTES];
	inline static uint32_t fatLoaded = NOTHING_LOADED;
	inline static uint8_t directoryBuffer[cock::abi::fat32::SECTOR_BYTES];
	inline static uint32_t directoryLoaded = NOTHING_LOADED;
	inline static uint8_t dataBuffer[cock::abi::fat32::SECTOR_BYTES];
	inline static uint8_t fsInfoBuffer[cock::abi::fat32::SECTOR_BYTES];
	inline static uint8_t zeros[cock::abi::block::WINDOW_BYTES];
	inline static cock::user::Clock clock;
	inline static bool clockReady = false;
	inline static bool fsInfoValid = false;
	inline static bool fsInfoDirty = false;
	inline static bool freeKnown = false;
	inline static uint32_t freeCount = 0;
	inline static uint32_t nextHint = cock::abi::fat32::FIRST_DATA_CLUSTER;

	static bool writable() { return !readOnly; }

	static bool sameFile(const Node &first, const Node &second) {
		return sameSpot(first.entry, second.entry);
	}

	static uint32_t spotSector(const Spot &spot) {
		return bpb.clusterSector(spot.cluster) + spot.sector;
	}

	static bool loadFat(uint32_t sector) {
		if (sector == fatLoaded) return true;
		fatLoaded = NOTHING_LOADED;
		if (!disk.read(device, sector, SINGLE_SECTOR, fatBuffer)) return false;
		fatLoaded = sector;
		return true;
	}

	static bool storeFat() {
		for (uint32_t copy = 0; copy < bpb.fatCount; copy++) {
			if (!disk.write(device, fatLoaded + copy * bpb.fatSectors, SINGLE_SECTOR,
							fatBuffer)) {
				fatLoaded = NOTHING_LOADED;
				return false;
			}
		}
		return true;
	}

	static uint32_t fatSector(uint32_t cluster) {
		return bpb.reservedSectors + cluster / cock::abi::fat32::FAT_ENTRIES_PER_SECTOR;
	}

	static bool setLink(uint32_t cluster, uint32_t value) {
		if (!loadFat(fatSector(cluster))) return false;
		cock::abi::fat32::setFatValue(
			fatBuffer, cluster % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR, value);
		return storeFat();
	}

	static Link nextCluster(uint32_t cluster, uint32_t &next) {
		if (!loadFat(fatSector(cluster))) return Link::BAD;

		uint32_t value = cock::abi::fat32::fatValue(
			fatBuffer, cluster % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR);
		if (cock::abi::fat32::isEndOfChain(value)) return Link::END;
		if (!bpb.isDataCluster(value)) return Link::BAD;
		next = value;
		return Link::NEXT;
	}

	static bool loadDirectory(uint32_t sector) {
		if (sector == directoryLoaded) return true;
		directoryLoaded = NOTHING_LOADED;
		if (!disk.read(device, sector, SINGLE_SECTOR, directoryBuffer)) return false;
		directoryLoaded = sector;
		return true;
	}

	static bool storeDirectory(uint32_t sector) {
		if (disk.write(device, sector, SINGLE_SECTOR, directoryBuffer)) return true;
		directoryLoaded = NOTHING_LOADED;
		return false;
	}

	static Walk nextRaw(Cursor &cursor, const uint8_t *&raw, Spot &here) {
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

	static bool nextEntry(Cursor &cursor, DirectoryParser &parser, DirEntry &entry) {
		const uint8_t *raw = nullptr;
		Spot here = {};
		while (nextRaw(cursor, raw, here) == Walk::ENTRY) {
			DirectoryParser::Result result = parser.feed(raw, entry);
			if (result == DirectoryParser::Result::END) return false;
			if (result == DirectoryParser::Result::ENTRY) return true;
		}
		return false;
	}

	static bool findInDirectory(uint32_t directory, const char *name, Found &found) {
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

	static bool walk(const char *path, Node &node, Found &leaf) {
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

	static bool resolve(const char *path, Node &node) {
		Found leaf = {};
		return walk(path, node, leaf);
	}

	static bool resolveLeaf(const char *path, Node &node) { return resolve(path, node); }

	static bool clusterAt(Handle &handle, uint32_t index, uint32_t &cluster) {
		uint32_t at = 0;
		uint32_t current = handle.node.cluster;
		if (handle.cursorCluster != NO_CLUSTER && handle.cursorIndex <= index) {
			at = handle.cursorIndex;
			current = handle.cursorCluster;
		}

		while (at < index) {
			uint32_t next = NO_CLUSTER;
			if (nextCluster(current, next) != Link::NEXT) return false;
			current = next;
			at++;
		}
		handle.cursorIndex = at;
		handle.cursorCluster = current;
		cluster = current;
		return true;
	}

	static bool read(Handle &handle, uint8_t *destination, uint32_t offset, uint32_t count) {
		const uint32_t clusterBytes = bpb.clusterBytes();
		uint32_t done = 0;
		while (done < count) {
			uint32_t position = offset + done;
			uint32_t cluster = NO_CLUSTER;
			if (!clusterAt(handle, position / clusterBytes, cluster)) return false;

			uint32_t within = position % clusterBytes;
			uint32_t sectorInCluster = within / cock::abi::fat32::SECTOR_BYTES;
			uint32_t byteInSector = within % cock::abi::fat32::SECTOR_BYTES;
			uint32_t sector = bpb.clusterSector(cluster) + sectorInCluster;
			uint32_t remaining = count - done;

			if (byteInSector == 0 && remaining >= cock::abi::fat32::SECTOR_BYTES) {
				uint32_t run = remaining / cock::abi::fat32::SECTOR_BYTES;
				if (run > bpb.sectorsPerCluster - sectorInCluster)
					run = bpb.sectorsPerCluster - sectorInCluster;
				if (run > cock::abi::block::MAX_SECTORS_PER_READ)
					run = cock::abi::block::MAX_SECTORS_PER_READ;
				if (!disk.read(device, sector, run, destination + done)) return false;
				done += run * static_cast<uint32_t>(cock::abi::fat32::SECTOR_BYTES);
			} else {
				if (!disk.read(device, sector, SINGLE_SECTOR, dataBuffer)) return false;
				uint32_t take =
					static_cast<uint32_t>(cock::abi::fat32::SECTOR_BYTES) - byteInSector;
				if (take > remaining) take = remaining;
				memcpy(destination + done, dataBuffer + byteInSector, take);
				done += take;
			}
		}
		return true;
	}

	static bool list(const Node &directory, uint32_t wanted, char *name, size_t capacity,
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

	static void flushFsInfo() {
		if (!fsInfoValid || !fsInfoDirty) return;
		fsInfoDirty = false;
		cock::abi::fat32::storeFsInfo(
			fsInfoBuffer, freeKnown ? freeCount : cock::abi::fat32::FSINFO_UNKNOWN,
			nextHint);
		disk.write(device, bpb.fsInfoSector, SINGLE_SECTOR, fsInfoBuffer);
	}

	static Status allocateCluster(uint32_t previous, uint32_t &cluster) {
		uint32_t start = bpb.isDataCluster(nextHint) ? nextHint
														: cock::abi::fat32::FIRST_DATA_CLUSTER;
		for (uint32_t step = 0; step < bpb.clusterCount; step++) {
			uint32_t candidate = cock::abi::fat32::FIRST_DATA_CLUSTER +
								 (start - cock::abi::fat32::FIRST_DATA_CLUSTER + step) %
									 bpb.clusterCount;
			if (!loadFat(fatSector(candidate))) return Status::IO_ERROR;
			uint32_t index = candidate % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR;
			if (cock::abi::fat32::fatValue(fatBuffer, index) != cock::abi::fat32::FAT_FREE)
				continue;

			cock::abi::fat32::setFatValue(fatBuffer, index,
										  cock::abi::fat32::FAT_END_OF_CHAIN);
			bool linked = false;
			if (previous != NO_CLUSTER && fatSector(previous) == fatLoaded) {
				cock::abi::fat32::setFatValue(
					fatBuffer, previous % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR,
					candidate);
				linked = true;
			}
			if (!storeFat()) return Status::IO_ERROR;
			if (previous != NO_CLUSTER && !linked && !setLink(previous, candidate))
				return Status::IO_ERROR;

			nextHint = candidate + 1;
			if (freeKnown && freeCount > 0) freeCount--;
			fsInfoDirty = true;
			cluster = candidate;
			return Status::OK;
		}
		return Status::NO_SPACE;
	}

	static bool freeChain(uint32_t first) {
		uint32_t cluster = first;
		uint32_t steps = 0;
		while (bpb.isDataCluster(cluster)) {
			uint32_t next = NO_CLUSTER;
			Link link = nextCluster(cluster, next);
			if (link == Link::BAD) return false;
			if (!setLink(cluster, cock::abi::fat32::FAT_FREE)) return false;

			if (freeKnown) freeCount++;
			if (cluster < nextHint) nextHint = cluster;
			fsInfoDirty = true;
			if (link == Link::END) return true;
			cluster = next;
			if (++steps > bpb.clusterCount) return false;
		}
		return false;
	}

	static bool zeroCluster(uint32_t cluster) {
		directoryLoaded = NOTHING_LOADED;
		uint32_t sector = bpb.clusterSector(cluster);
		uint32_t left = bpb.sectorsPerCluster;
		while (left > 0) {
			uint32_t run = left < cock::abi::block::MAX_SECTORS_PER_WRITE
							   ? left
							   : cock::abi::block::MAX_SECTORS_PER_WRITE;
			if (!disk.write(device, sector, run, zeros)) return false;
			sector += run;
			left -= run;
		}
		return true;
	}

	static void currentStamp(uint16_t &date, uint16_t &time) {
		cock::abi::datetime::DateTime now = {};
		if (clockReady && clock.now(now)) {
			date = cock::abi::datetime::toFatDate(now);
			time = cock::abi::datetime::toFatTime(now);
			return;
		}
		date = cock::abi::fat32::FAT_EPOCH_DATE;
		time = cock::abi::fat32::FAT_EPOCH_TIME;
	}

	static bool isFreeEntry(uint8_t first) {
		return first == cock::abi::fat32::DIRECTORY_END ||
			   first == cock::abi::fat32::DIRECTORY_DELETED;
	}

	static Status findFreeRun(uint32_t directory, size_t needed, Spot &start) {
		Cursor cursor = {directory, 0, 0, 0};
		const uint8_t *raw = nullptr;
		Spot here = {};
		Spot runStart = {};
		size_t run = 0;
		for (;;) {
			Walk step = nextRaw(cursor, raw, here);
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
		const size_t perCluster = size_t(bpb.sectorsPerCluster) * cock::abi::fat32::ENTRIES_PER_SECTOR;
		while (run < needed) {
			uint32_t added = NO_CLUSTER;
			Status allocated = allocateCluster(NO_CLUSTER, added);
			if (allocated != Status::OK) return allocated;
			if (!zeroCluster(added) || !setLink(last, added)) return Status::IO_ERROR;
			if (run == 0) runStart = {added, 0, 0};
			run += perCluster;
			last = added;
		}
		start = runStart;
		return Status::OK;
	}

	static Status shortNameTaken(uint32_t directory, const uint8_t *shortName, bool &taken) {
		taken = false;
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
				cock::abi::fat32::ATTRIBUTE_LONG_NAME)
				continue;
			if (memcmp(raw, shortName, cock::abi::fat32::ENTRY_SHORT_NAME_BYTES) == 0) {
				taken = true;
				return Status::OK;
			}
		}
	}

	static Status writeEntryAt(const Spot &spot, const uint8_t *entry) {
		uint32_t sector = spotSector(spot);
		if (!loadDirectory(sector)) return Status::IO_ERROR;
		memcpy(directoryBuffer + size_t(spot.entry) * cock::abi::fat32::ENTRY_BYTES, entry,
			   cock::abi::fat32::ENTRY_BYTES);
		return storeDirectory(sector) ? Status::OK : Status::IO_ERROR;
	}

	static Status placeEntry(uint32_t directory, const char *name, size_t nameBytes,
							 const uint8_t *entryTemplate, Spot &where) {
		uint8_t shortName[cock::abi::fat32::ENTRY_SHORT_NAME_BYTES];
		uint8_t caseFlags = 0;
		cock::abi::fat32::NamePlan plan =
			cock::abi::fat32::planName(name, nameBytes, shortName, caseFlags);

		uint16_t units[cock::abi::fat32::MAX_NAME_CHARS];
		size_t unitCount = 0;
		size_t longEntries = 0;
		if (plan != cock::abi::fat32::NamePlan::SHORT) {
			if (!cock::abi::fat32::decodeUtf8Name(name, nameBytes, units, cock::abi::fat32::MAX_NAME_CHARS,
												  unitCount))
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
			cock::abi::fat32::encodeLongEntry(entry, longEntries - index, index == 0, checksum,
											  units, unitCount);
			written = writeEntryAt(at, entry);
			if (written != Status::OK) return written;
			if (!stepSpot(at)) return Status::IO_ERROR;
		}
		where = shortSpot;
		return Status::OK;
	}

	static void buildTemplate(uint8_t *entry, uint8_t attributes, uint32_t cluster) {
		uint8_t blank[cock::abi::fat32::ENTRY_SHORT_NAME_BYTES];
		memset(blank, cock::abi::fat32::SPACE, sizeof(blank));
		uint16_t date = 0;
		uint16_t time = 0;
		currentStamp(date, time);
		cock::abi::fat32::encodeEntry(entry, blank, 0, attributes, cluster, NO_BYTES, date, time);
	}

	static Status commitEntry(const Node &node) {
		uint32_t sector = spotSector(node.entry);
		if (!loadDirectory(sector)) return Status::IO_ERROR;

		uint8_t *raw =
			directoryBuffer + size_t(node.entry.entry) * cock::abi::fat32::ENTRY_BYTES;
		uint16_t date = 0;
		uint16_t time = 0;
		currentStamp(date, time);
		cock::abi::fat32::patchEntryLocation(raw, node.cluster, node.size);
		cock::abi::fat32::touchEntry(raw, date, time);
		return storeDirectory(sector) ? Status::OK : Status::IO_ERROR;
	}

	static bool stepSpot(Spot &spot) {
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

	static Status parentAndName(const char *path, Node &parent, char *name, size_t &nameBytes) {
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

	static bool setCleanBit(bool clean) {
		if (!loadFat(fatSector(cock::abi::fat32::FAT_STATE_ENTRY))) return false;
		uint8_t *entry = fatBuffer + size_t(cock::abi::fat32::FAT_STATE_ENTRY) *
										 cock::abi::fat32::FAT_ENTRY_BYTES;
		uint32_t value = cock::abi::fat32::detail::readLe32(entry);
		value = clean ? (value | cock::abi::fat32::FAT_CLEAN_SHUTDOWN_BIT)
					  : (value & ~cock::abi::fat32::FAT_CLEAN_SHUTDOWN_BIT);
		cock::abi::fat32::detail::writeLe32(entry, value);
		return storeFat();
	}

	static bool isCleanVolume() {
		if (!loadFat(fatSector(cock::abi::fat32::FAT_STATE_ENTRY))) return false;
		const uint8_t *entry = fatBuffer + size_t(cock::abi::fat32::FAT_STATE_ENTRY) *
											   cock::abi::fat32::FAT_ENTRY_BYTES;
		return (cock::abi::fat32::detail::readLe32(entry) &
				cock::abi::fat32::FAT_CLEAN_SHUTDOWN_BIT) != 0;
	}

	static bool begin() { return setCleanBit(false); }

	static Status end(Status status) { return setCleanBit(true) ? status : Status::IO_ERROR; }

	static Status create(const char *path, Node &node) {
		if (!begin()) return Status::IO_ERROR;
		return end(createEntry(path, node));
	}

	static Status makeDirectory(const char *path) {
		if (!begin()) return Status::IO_ERROR;
		return end(makeDirectoryEntry(path));
	}

	static Status removeDirectory(const char *path) {
		if (!begin()) return Status::IO_ERROR;
		return end(removeDirectoryEntry(path));
	}

	static Status rename(const char *from, const char *to) {
		if (!begin()) return Status::IO_ERROR;
		return end(renameEntry(from, to));
	}

	static Status truncate(Node &node) {
		if (!begin()) return Status::IO_ERROR;
		return end(truncateNode(node));
	}

	static Status write(Handle &handle, const uint8_t *source, uint32_t offset, uint32_t count,
						uint32_t &written) {
		if (!begin()) return Status::IO_ERROR;
		return end(writeBytes(handle, source, offset, count, written));
	}

	static Status unlink(const char *path) {
		if (!begin()) return Status::IO_ERROR;
		return end(unlinkEntry(path));
	}

	static Status createEntry(const char *path, Node &node) {
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

	static Status makeDirectoryEntry(const char *path) {
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

	static Status deleteEntries(const Found &leaf) {
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

	static Status isEmptyDirectory(uint32_t directory, bool &empty) {
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

	static Status removeDirectoryEntry(const char *path) {
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

	static bool nextComponent(const char *&cursor, char *component, size_t capacity) {
		while (*cursor == PATH_SEPARATOR)
			cursor++;
		if (*cursor == '\0') return false;

		size_t length = 0;
		while (*cursor != '\0' && *cursor != PATH_SEPARATOR) {
			if (length + 1 >= capacity) return false;
			component[length++] = *cursor++;
		}
		component[length] = '\0';
		return true;
	}

	static bool isInside(const char *ancestor, const char *path) {
		char first[cock::abi::fs::MAX_PATH_BYTES];
		char second[cock::abi::fs::MAX_PATH_BYTES];
		while (nextComponent(ancestor, first, sizeof(first))) {
			if (!nextComponent(path, second, sizeof(second)) ||
				!cock::abi::fat32::namesEqual(first, second))
				return false;
		}
		return true;
	}

	static Status link(const char *, const char *) { return Status::UNSUPPORTED; }

	static Status checkReplace(const Node &source, const Found &existing) {
		bool sourceIsDirectory = source.type == FileType::DIRECTORY;
		if (existing.entry.isDirectory() != sourceIsDirectory) return Status::EXISTS;
		if (!sourceIsDirectory) return Status::OK;
		if (!bpb.isDataCluster(existing.entry.cluster)) return Status::IO_ERROR;

		bool empty = false;
		Status scanned = isEmptyDirectory(existing.entry.cluster, empty);
		if (scanned != Status::OK) return scanned;
		return empty ? Status::OK : Status::NOT_EMPTY;
	}

	static Status removeFound(const Found &existing) {
		Status deleted = deleteEntries(existing);
		if (deleted != Status::OK) return deleted;
		bool freed = !bpb.isDataCluster(existing.entry.cluster) || freeChain(existing.entry.cluster);
		flushFsInfo();
		return freed ? Status::OK : Status::IO_ERROR;
	}

	static Status renameEntry(const char *from, const char *to) {
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
			if (placed == Status::OK) placed = placeEntry(parent.cluster, name, targetBytes, entry, where);
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

	static Status truncateNode(Node &node) {
		uint32_t first = node.cluster;
		uint32_t size = node.size;
		node.cluster = NO_CLUSTER;
		node.size = NO_BYTES;

		Status committed = commitEntry(node);
		if (committed != Status::OK) {
			node.cluster = first;
			node.size = size;
			return committed;
		}

		bool freed = first == NO_CLUSTER || freeChain(first);
		flushFsInfo();
		return freed ? Status::OK : Status::IO_ERROR;
	}

	static Status clusterForWrite(Handle &handle, uint32_t index, uint32_t &cluster) {
		Node &node = handle.node;
		if (node.cluster == NO_CLUSTER) {
			if (index != 0) return Status::IO_ERROR;
			Status made = allocateCluster(NO_CLUSTER, cluster);
			if (made != Status::OK) return made;
			node.cluster = cluster;
			handle.cursorIndex = 0;
			handle.cursorCluster = cluster;
			return Status::OK;
		}

		uint32_t at = 0;
		uint32_t current = node.cluster;
		if (handle.cursorCluster != NO_CLUSTER && handle.cursorIndex <= index) {
			at = handle.cursorIndex;
			current = handle.cursorCluster;
		}

		while (at < index) {
			uint32_t next = NO_CLUSTER;
			Link link = nextCluster(current, next);
			if (link == Link::END) {
				Status added = allocateCluster(current, next);
				if (added != Status::OK) return added;
			} else if (link != Link::NEXT) {
				return Status::IO_ERROR;
			}
			current = next;
			at++;
		}
		handle.cursorIndex = at;
		handle.cursorCluster = current;
		cluster = current;
		return Status::OK;
	}

	static Status writeBytes(Handle &handle, const uint8_t *source, uint32_t offset,
							 uint32_t count, uint32_t &written) {
		Node &node = handle.node;
		const uint32_t clusterBytes = bpb.clusterBytes();
		const uint32_t originalCluster = node.cluster;
		const uint32_t originalSize = node.size;
		uint32_t done = 0;
		Status result = Status::OK;

		while (done < count) {
			uint32_t position = offset + done;
			uint32_t cluster = NO_CLUSTER;
			result = clusterForWrite(handle, position / clusterBytes, cluster);
			if (result != Status::OK) break;

			uint32_t within = position % clusterBytes;
			uint32_t sectorInCluster = within / cock::abi::fat32::SECTOR_BYTES;
			uint32_t byteInSector = within % cock::abi::fat32::SECTOR_BYTES;
			uint32_t sector = bpb.clusterSector(cluster) + sectorInCluster;
			uint32_t remaining = count - done;
			uint32_t take = 0;

			if (byteInSector == 0 && remaining >= cock::abi::fat32::SECTOR_BYTES) {
				uint32_t run = remaining / cock::abi::fat32::SECTOR_BYTES;
				if (run > bpb.sectorsPerCluster - sectorInCluster)
					run = bpb.sectorsPerCluster - sectorInCluster;
				if (run > cock::abi::block::MAX_SECTORS_PER_WRITE)
					run = cock::abi::block::MAX_SECTORS_PER_WRITE;
				if (!disk.write(device, sector, run, source + done)) {
					result = Status::IO_ERROR;
					break;
				}
				take = run * static_cast<uint32_t>(cock::abi::fat32::SECTOR_BYTES);
			} else {
				if (position - byteInSector >= node.size) {
					memset(dataBuffer, 0, sizeof(dataBuffer));
				} else if (!disk.read(device, sector, SINGLE_SECTOR, dataBuffer)) {
					result = Status::IO_ERROR;
					break;
				}
				take = static_cast<uint32_t>(cock::abi::fat32::SECTOR_BYTES) - byteInSector;
				if (take > remaining) take = remaining;
				memcpy(dataBuffer + byteInSector, source + done, take);
				if (!disk.write(device, sector, SINGLE_SECTOR, dataBuffer)) {
					result = Status::IO_ERROR;
					break;
				}
			}

			done += take;
			if (offset + done > node.size) node.size = offset + done;
		}

		written = done;
		if (node.size != originalSize || node.cluster != originalCluster) {
			Status committed = commitEntry(node);
			if (committed != Status::OK) result = committed;
		}
		flushFsInfo();
		if (done > 0 && result == Status::NO_SPACE) result = Status::OK;
		return result;
	}

	static Status unlinkEntry(const char *path) {
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

	static bool mount(uint32_t candidate) {
		if (!disk.connect() || candidate >= disk.deviceCount()) return false;

		cock::user::block::DeviceInfo info = {};
		Bpb found = {};
		if (!disk.info(candidate, info) ||
			!disk.read(candidate, SECTOR_ZERO, SINGLE_SECTOR, dataBuffer))
			return false;
		if (cock::abi::fat32::parseBpb(dataBuffer, info.sectorCount, found) !=
			cock::abi::fat32::BpbError::NONE)
			return false;

		bpb = found;
		device = candidate;
		readOnly = info.kind == cock::abi::block::DeviceKind::DISK || !isCleanVolume();
		clockReady = clock.init();
		loadFsInfo();
		return true;
	}

	static void loadFsInfo() {
		fsInfoValid = false;
		freeKnown = false;
		nextHint = cock::abi::fat32::FIRST_DATA_CLUSTER;
		if (bpb.fsInfoSector < FIRST_FSINFO_SECTOR || bpb.fsInfoSector >= bpb.reservedSectors)
			return;
		if (!disk.read(device, bpb.fsInfoSector, SINGLE_SECTOR, fsInfoBuffer) ||
			!cock::abi::fat32::validFsInfo(fsInfoBuffer))
			return;

		fsInfoValid = true;
		uint32_t known = cock::abi::fat32::fsInfoFree(fsInfoBuffer);
		freeKnown = known != cock::abi::fat32::FSINFO_UNKNOWN && known <= bpb.clusterCount;
		freeCount = freeKnown ? known : 0;
		uint32_t hint = cock::abi::fat32::fsInfoNext(fsInfoBuffer);
		if (bpb.isDataCluster(hint)) nextHint = hint;
	}
};

using Server = cock::user::FileServer<Fat>;

} // namespace

extern "C" void _start() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) break;
		Server::defer(message);
	}

	Fat::disk.onForeign = Server::defer;
	Fat::clock.onForeign = Server::defer;
	uint32_t wanted = message.payload.args.arg1;
	if (!Fat::mount(wanted)) {
		printf("fat: device %u is not a FAT32 volume\n", wanted);
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("fat: device %u, %u clusters of %u sectors\n", Fat::device,
		   Fat::bpb.clusterCount, Fat::bpb.sectorsPerCluster);

	Server::run();
}
