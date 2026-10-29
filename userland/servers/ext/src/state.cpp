#include <ext/stamp.hpp>
#include <ext/state.hpp>
#include <ext/volume.hpp>

using cock::abi::fs::Status;

namespace {

bool flushSuperblock(bool valid) {
	using namespace cock::ext;
	uint32_t state = valid ? (sb.state | cock::abi::ext2::STATE_VALID)
						   : (sb.state & ~cock::abi::ext2::STATE_VALID);
	cock::abi::ext2::detail::writeLe32(
		superblockBuffer + cock::abi::ext2::SB_FREE_BLOCKS_OFFSET, sb.freeBlocks);
	cock::abi::ext2::detail::writeLe32(
		superblockBuffer + cock::abi::ext2::SB_FREE_INODES_OFFSET, sb.freeInodes);
	cock::abi::ext2::detail::writeLe16(superblockBuffer + cock::abi::ext2::SB_STATE_OFFSET,
									   state);
	if (valid)
		cock::abi::ext2::detail::writeLe32(
			superblockBuffer + cock::abi::ext2::SB_WRITE_TIME_OFFSET, timestamp());
	return disk.write(device, cock::abi::ext2::SUPERBLOCK_SECTOR, SINGLE_SECTOR,
					  superblockBuffer);
}

} // namespace

namespace cock::ext {

bool beginUpdate() { return flushSuperblock(false); }

Status endUpdate(Status status) { return flushSuperblock(true) ? status : Status::IO_ERROR; }

} // namespace cock::ext
