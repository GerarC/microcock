#include <ext/inode.hpp>
#include <ext/mount.hpp>
#include <ext/server.hpp>
#include <ext/volume.hpp>
#include <stdio.h>

using cock::abi::fs::FileType;

namespace {

void report(cock::abi::ext2::Error error, uint32_t candidate) {
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
			if ((cock::ext::sb.unsupported & bit) != 0)
				printf("ext: device %u: unsupported feature %s\n", candidate,
					   cock::abi::ext2::featureName(bit));
		return;
	default:
		return;
	}
}

} // namespace

namespace cock::ext {

bool mountVolume(uint32_t candidate) {
	disk.onForeign = Server::defer;
	clock.onForeign = Server::defer;
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
	readOnly = info.kind == cock::abi::block::DeviceKind::DISK;
	clockReady = clock.init();
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
	if (!writable()) printf("ext: device %u is mounted read only\n", candidate);
	return true;
}

} // namespace cock::ext
