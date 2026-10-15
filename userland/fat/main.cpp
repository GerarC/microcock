#include <cock/abi/block.hpp>
#include <cock/abi/fat32.hpp>
#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/block.hpp>
#include <cock/user/file_server.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using cock::abi::fat32::Bpb;
using cock::abi::fat32::DirectoryParser;
using cock::abi::fat32::DirEntry;
using cock::abi::fs::FileType;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

constexpr uintptr_t EXIT_FAILED_STATUS = 1;
constexpr uint32_t NO_CLUSTER = 0;
constexpr uint32_t NOTHING_LOADED = 0xFFFFFFFF;
constexpr uint32_t SECTOR_ZERO = 0;
constexpr uint32_t SINGLE_SECTOR = 1;
constexpr uint32_t NO_BYTES = 0;
constexpr char PATH_SEPARATOR = '/';

struct Cursor {
	uint32_t cluster;
	uint32_t sector;
	uint32_t entry;
	uint32_t steps;
};

enum class Link : uint32_t {
	NEXT,
	END,
	BAD,
};

struct Fat {
	struct Node {
		uint32_t cluster;
		uint32_t size;
		FileType type;
	};

	struct Handle {
		bool open;
		Node node;
		uint32_t cursorIndex;
		uint32_t cursorCluster;
	};

	inline static cock::user::block::Client disk;
	inline static uint32_t device = 0;
	inline static Bpb bpb = {};
	inline static uint8_t fatBuffer[cock::abi::fat32::SECTOR_BYTES];
	inline static uint32_t fatLoaded = NOTHING_LOADED;
	inline static uint8_t directoryBuffer[cock::abi::fat32::SECTOR_BYTES];
	inline static uint32_t directoryLoaded = NOTHING_LOADED;
	inline static uint8_t dataBuffer[cock::abi::fat32::SECTOR_BYTES];

	static Link nextCluster(uint32_t cluster, uint32_t &next) {
		uint32_t sector =
			bpb.reservedSectors + cluster / cock::abi::fat32::FAT_ENTRIES_PER_SECTOR;
		if (sector != fatLoaded) {
			fatLoaded = NOTHING_LOADED;
			if (!disk.read(device, sector, SINGLE_SECTOR, fatBuffer)) return Link::BAD;
			fatLoaded = sector;
		}

		uint32_t value = cock::abi::fat32::fatValue(
			fatBuffer, cluster % cock::abi::fat32::FAT_ENTRIES_PER_SECTOR);
		if (cock::abi::fat32::isEndOfChain(value)) return Link::END;
		if (!bpb.isDataCluster(value)) return Link::BAD;
		next = value;
		return Link::NEXT;
	}

	static bool nextEntry(Cursor &cursor, DirectoryParser &parser, DirEntry &entry) {
		for (;;) {
			if (cursor.entry == cock::abi::fat32::ENTRIES_PER_SECTOR) {
				cursor.entry = 0;
				cursor.sector++;
			}
			if (cursor.sector == bpb.sectorsPerCluster) {
				uint32_t next = NO_CLUSTER;
				if (nextCluster(cursor.cluster, next) != Link::NEXT) return false;
				if (++cursor.steps > bpb.clusterCount) return false;
				cursor.cluster = next;
				cursor.sector = 0;
			}

			uint32_t sector = bpb.clusterSector(cursor.cluster) + cursor.sector;
			if (sector != directoryLoaded) {
				directoryLoaded = NOTHING_LOADED;
				if (!disk.read(device, sector, SINGLE_SECTOR, directoryBuffer))
					return false;
				directoryLoaded = sector;
			}

			const uint8_t *raw =
				directoryBuffer + size_t(cursor.entry) * cock::abi::fat32::ENTRY_BYTES;
			cursor.entry++;
			DirectoryParser::Result result = parser.feed(raw, entry);
			if (result == DirectoryParser::Result::END) return false;
			if (result == DirectoryParser::Result::ENTRY) return true;
		}
	}

	static bool findInDirectory(uint32_t directory, const char *name, DirEntry &found) {
		DirectoryParser parser;
		parser.reset();
		Cursor cursor = {directory, 0, 0, 0};
		while (nextEntry(cursor, parser, found))
			if (cock::abi::fat32::namesEqual(found.name, name)) return true;
		return false;
	}

	static bool resolve(const char *path, Node &node) {
		node = {bpb.rootCluster, NO_BYTES, FileType::DIRECTORY};
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
			DirEntry entry = {};
			if (!findInDirectory(node.cluster, component, entry)) return false;

			if (entry.isDirectory()) {
				if (!bpb.isDataCluster(entry.cluster)) return false;
				node = {entry.cluster, NO_BYTES, FileType::DIRECTORY};
			} else {
				if (entry.size != NO_BYTES && !bpb.isDataCluster(entry.cluster))
					return false;
				node = {entry.cluster, entry.size, FileType::REGULAR};
			}
		}
		return true;
	}

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
		return true;
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
	uint32_t wanted = message.payload.args.arg1;
	if (!Fat::mount(wanted)) {
		printf("fat: device %u is not a FAT32 volume\n", wanted);
		cock::user::exit_process(EXIT_FAILED_STATUS);
	}
	printf("fat: device %u, %u clusters of %u sectors\n", Fat::device,
		   Fat::bpb.clusterCount, Fat::bpb.sectorsPerCluster);

	Server::run();
}
