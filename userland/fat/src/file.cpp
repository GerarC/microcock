#include <fat/directory.hpp>
#include <fat/file.hpp>
#include <fat/fsinfo.hpp>
#include <fat/table.hpp>
#include <fat/volume.hpp>
#include <string.h>

using cock::abi::fs::Status;
using cock::fat::Handle;
using cock::fat::Link;
using cock::fat::Node;

namespace {

bool clusterAt(Handle &handle, uint32_t index, uint32_t &cluster) {
	uint32_t at = 0;
	uint32_t current = handle.node.cluster;
	if (handle.cursorCluster != cock::fat::NO_CLUSTER && handle.cursorIndex <= index) {
		at = handle.cursorIndex;
		current = handle.cursorCluster;
	}

	while (at < index) {
		uint32_t next = cock::fat::NO_CLUSTER;
		if (cock::fat::nextCluster(current, next) != Link::NEXT) return false;
		current = next;
		at++;
	}
	handle.cursorIndex = at;
	handle.cursorCluster = current;
	cluster = current;
	return true;
}

Status clusterForWrite(Handle &handle, uint32_t index, uint32_t &cluster) {
	Node &node = handle.node;
	if (node.cluster == cock::fat::NO_CLUSTER) {
		if (index != 0) return Status::IO_ERROR;
		Status made = cock::fat::allocateCluster(cock::fat::NO_CLUSTER, cluster);
		if (made != Status::OK) return made;
		node.cluster = cluster;
		handle.cursorIndex = 0;
		handle.cursorCluster = cluster;
		return Status::OK;
	}

	uint32_t at = 0;
	uint32_t current = node.cluster;
	if (handle.cursorCluster != cock::fat::NO_CLUSTER && handle.cursorIndex <= index) {
		at = handle.cursorIndex;
		current = handle.cursorCluster;
	}

	while (at < index) {
		uint32_t next = cock::fat::NO_CLUSTER;
		Link link = cock::fat::nextCluster(current, next);
		if (link == Link::END) {
			Status added = cock::fat::allocateCluster(current, next);
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

} // namespace

namespace cock::fat {

bool readBytes(Handle &handle, uint8_t *destination, uint32_t offset, uint32_t count) {
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
			uint32_t take = static_cast<uint32_t>(cock::abi::fat32::SECTOR_BYTES) - byteInSector;
			if (take > remaining) take = remaining;
			memcpy(destination + done, dataBuffer + byteInSector, take);
			done += take;
		}
	}
	return true;
}

Status writeBytes(Handle &handle, const uint8_t *source, uint32_t offset, uint32_t count,
				  uint32_t &written) {
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

Status truncateNode(Node &node) {
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

} // namespace cock::fat
