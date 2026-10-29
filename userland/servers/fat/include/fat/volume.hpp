/** @file
 * @brief The mounted volume: its geometry, the sector buffers and the positions used to walk it.
 */
#ifndef FAT_VOLUME_HPP
#define FAT_VOLUME_HPP

#include <cock/abi/block.hpp>
#include <cock/abi/fat32.hpp>
#include <cock/abi/fs.hpp>
#include <cock/user/block.hpp>
#include <cock/user/clock.hpp>
#include <stdint.h>

namespace cock::fat {

/** @brief Cluster number that means "no cluster": an empty file has no chain. */
constexpr uint32_t NO_CLUSTER = 0;

/** @brief Value of a buffer's loaded sector while the buffer holds nothing valid. */
constexpr uint32_t NOTHING_LOADED = 0xFFFFFFFF;

/** @brief Sector count of a request for one sector. */
constexpr uint32_t SINGLE_SECTOR = 1;

/** @brief Size and byte count of an empty file or of a directory. */
constexpr uint32_t NO_BYTES = 0;

/** @brief Character that separates the components of a path. */
constexpr char PATH_SEPARATOR = '/';

/**
 * @brief A position while walking the entries of a directory.
 */
struct Cursor {
	uint32_t cluster; /**< Cluster of the directory chain being read. */
	uint32_t sector; /**< Sector inside the cluster. */
	uint32_t entry; /**< Next entry inside the sector. */
	uint32_t steps; /**< Clusters followed so far, bounded by the cluster count of the volume. */
};

/**
 * @brief The place of one directory entry.
 */
struct Spot {
	uint32_t cluster; /**< Cluster of the directory chain that holds the entry. */
	uint32_t sector; /**< Sector inside the cluster. */
	uint32_t entry; /**< Entry inside the sector. */
};

/**
 * @brief A directory entry found by name, with the places of its entries.
 */
struct Found {
	cock::abi::fat32::DirEntry entry; /**< The decoded entry. */
	Spot shortEntry; /**< Place of the short entry. */
	Spot firstEntry; /**< Place of the first long entry of the name, or of the short entry. */
};

/**
 * @brief What follows a cluster in its chain.
 */
enum class Link : uint32_t {
	NEXT, /**< Another data cluster. */
	END, /**< The end of the chain. */
	BAD, /**< A value that is not a data cluster, or a FAT sector that cannot be read. */
};

/**
 * @brief Result of one step of a directory walk.
 */
enum class Walk : uint32_t {
	ENTRY, /**< A raw entry is available. */
	END, /**< The chain of the directory ended. */
	FAILED, /**< A sector could not be read or the chain is not valid. */
};

/**
 * @brief A file or a directory of the volume.
 */
struct Node {
	uint32_t cluster; /**< First cluster, `NO_CLUSTER` for an empty file. */
	uint32_t size; /**< Size in bytes; zero for a directory. */
	cock::abi::fs::FileType type; /**< Regular file or directory. */
	Spot entry; /**< Place of its short entry, which identifies it. */
};

/**
 * @brief An open file of a client session.
 */
struct Handle {
	bool open; /**< True while the handle is in use. */
	Node node; /**< The file it refers to. */
	uint32_t cursorIndex; /**< Index in the chain of the cluster remembered in `cursorCluster`. */
	uint32_t cursorCluster; /**< Cluster at `cursorIndex`, or `NO_CLUSTER` when none is remembered. */
};

/** @brief The block server the volume lives on. */
extern cock::user::block::Client disk;

/** @brief The block device that holds the volume. */
extern uint32_t device;

/** @brief True when the volume is served read only. */
extern bool readOnly;

/** @brief Geometry of the volume, from its boot sector. */
extern cock::abi::fat32::Bpb bpb;

/** @brief Buffer for one sector of the FAT. */
extern uint8_t fatBuffer[cock::abi::fat32::SECTOR_BYTES];

/** @brief Sector held in `fatBuffer`, or `NOTHING_LOADED`. */
extern uint32_t fatLoaded;

/** @brief Buffer for one sector of a directory. */
extern uint8_t directoryBuffer[cock::abi::fat32::SECTOR_BYTES];

/** @brief Sector held in `directoryBuffer`, or `NOTHING_LOADED`. */
extern uint32_t directoryLoaded;

/** @brief Buffer for the partial sectors of file data and for the boot sector. */
extern uint8_t dataBuffer[cock::abi::fat32::SECTOR_BYTES];

/** @brief Zeroes written over a new directory cluster. */
extern uint8_t zeros[cock::abi::block::WINDOW_BYTES];

/** @brief Client of the clock server, the source of file times. */
extern cock::user::Clock clock;

/** @brief True when the clock server answered at mount time. */
extern bool clockReady;

/**
 * @brief Tells whether the volume accepts writes.
 *
 * @return True when it is not read only.
 */
inline bool writable() { return !readOnly; }

/**
 * @brief Compares two directory entry places.
 *
 * @param first One place.
 * @param second The other place.
 * @return True when both name the same entry.
 */
inline bool sameSpot(const Spot &first, const Spot &second) {
	return first.cluster == second.cluster && first.sector == second.sector &&
		   first.entry == second.entry;
}

/**
 * @brief Gives the volume sector that holds a directory entry.
 *
 * @param spot The place of the entry.
 * @return Sector number on the device.
 */
inline uint32_t spotSector(const Spot &spot) {
	return bpb.clusterSector(spot.cluster) + spot.sector;
}

} // namespace cock::fat

#endif // !FAT_VOLUME_HPP
