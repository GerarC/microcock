/** @file
 * @brief Reading and changing the entries of a directory.
 */
#ifndef FAT_DIRECTORY_HPP
#define FAT_DIRECTORY_HPP

#include <cock/abi/fs.hpp>
#include <fat/volume.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::fat {

/**
 * @brief Loads a sector of a directory into `directoryBuffer`.
 *
 * @param sector Sector number on the device.
 * @return True when the sector is loaded, false when it cannot be read.
 */
bool loadDirectory(uint32_t sector);

/**
 * @brief Writes `directoryBuffer` back to a sector.
 *
 * @param sector Sector number on the device.
 * @return True on success. On failure the buffer is forgotten.
 */
bool storeDirectory(uint32_t sector);

/**
 * @brief Moves a cursor to the next raw entry of a directory.
 *
 * Follows the chain of the directory and stops when it ends, when it loops or when a sector
 * cannot be read.
 *
 * @param cursor The position, advanced on `Walk::ENTRY`.
 * @param raw Receives a pointer into `directoryBuffer`, valid until the next sector is loaded.
 * @param here Receives the place of the entry.
 * @return `ENTRY`, `END` at the end of the chain, or `FAILED`.
 */
Walk nextRaw(Cursor &cursor, const uint8_t *&raw, Spot &here);

/**
 * @brief Moves a place to the next entry slot, following the chain of the directory.
 *
 * @param spot The place, advanced on success.
 * @return True on success, false when the chain ends or is not valid.
 */
bool stepSpot(Spot &spot);

/**
 * @brief Looks a name up in one directory.
 *
 * Compares with the long name when its checksum matches and with the short name otherwise,
 * ignoring ASCII case.
 *
 * @param directory First cluster of the directory.
 * @param name Name to find, terminated by zero.
 * @param found Receives the entry and the places of its entries.
 * @return True when found, false when absent or the directory cannot be read.
 */
bool findInDirectory(uint32_t directory, const char *name, Found &found);

/**
 * @brief Gives the name and type of the n-th entry of a directory.
 *
 * @param directory The directory.
 * @param wanted Index of the entry, counting only visible entries.
 * @param name Receives the name, terminated by zero.
 * @param capacity Size of `name` in bytes.
 * @param type Receives the type of the entry.
 * @return True on success, false when there is no such entry or the name does not fit.
 */
bool listEntry(const Node &directory, uint32_t wanted, char *name, size_t capacity,
			   cock::abi::fs::FileType &type);

/**
 * @brief Overwrites one entry slot and writes its sector.
 *
 * @param spot The place of the slot.
 * @param entry The raw entry, `ENTRY_BYTES` long.
 * @return `OK` or `IO_ERROR`.
 */
cock::abi::fs::Status writeEntryAt(const Spot &spot, const uint8_t *entry);

/**
 * @brief Marks the long entries and the short entry of a name as deleted.
 *
 * @param leaf The entry to delete, with the places of its entries.
 * @return `OK` or `IO_ERROR`.
 */
cock::abi::fs::Status deleteEntries(const Found &leaf);

/**
 * @brief Writes the first cluster and the size of a file into its directory entry.
 *
 * Also stamps the write time.
 *
 * @param node The file.
 * @return `OK` or `IO_ERROR`.
 */
cock::abi::fs::Status commitEntry(const Node &node);

/**
 * @brief Tells whether a directory holds nothing but `.` and `..`.
 *
 * @param directory First cluster of the directory.
 * @param empty Receives the answer when the result is `OK`.
 * @return `OK` or `IO_ERROR`.
 */
cock::abi::fs::Status isEmptyDirectory(uint32_t directory, bool &empty);

} // namespace cock::fat

#endif // !FAT_DIRECTORY_HPP
