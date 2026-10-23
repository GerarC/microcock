/** @file
 * @brief Adding entries to a directory: names, long entries and free slots.
 */
#ifndef FAT_ENTRIES_HPP
#define FAT_ENTRIES_HPP

#include <cock/abi/fs.hpp>
#include <fat/volume.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::fat {

/**
 * @brief Builds the raw entry used as the template of a new file or directory.
 *
 * The name is left blank for `placeEntry`; the creation and write times are the current time.
 *
 * @param entry Receives `ENTRY_BYTES` bytes.
 * @param attributes Attribute byte of the entry.
 * @param cluster First cluster, or `NO_CLUSTER`.
 */
void buildTemplate(uint8_t *entry, uint8_t attributes, uint32_t cluster);

/**
 * @brief Adds an entry to a directory.
 *
 * Plans the short name, makes a unique alias for a name that does not fit 8.3, finds a run of
 * free slots (growing the directory when needed) and writes the long entries and the short
 * entry.
 *
 * @param directory First cluster of the directory.
 * @param name Name in UTF-8.
 * @param nameBytes Length of the name.
 * @param entryTemplate Raw entry to copy, from `buildTemplate` or an entry being renamed.
 * @param where Receives the place of the short entry on success.
 * @return `OK`, `BAD_REQUEST` for a name that cannot be stored, `EXISTS` when the short name
 * is taken, `NO_SPACE`, or `IO_ERROR`.
 */
cock::abi::fs::Status placeEntry(uint32_t directory, const char *name, size_t nameBytes,
								 const uint8_t *entryTemplate, Spot &where);

} // namespace cock::fat

#endif // !FAT_ENTRIES_HPP
