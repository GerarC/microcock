#include <cock/abi/shm.hpp>
#include <cock/user/api.hpp>
#include <ramfs/archive.hpp>
#include <string.h>

using cock::abi::fs::FileType;

namespace cock::ramfs {

namespace {

constexpr size_t TAR_BLOCK_BYTES = 512;
constexpr size_t TAR_NAME_OFFSET = 0;
constexpr size_t TAR_SIZE_OFFSET = 124;
constexpr size_t TAR_SIZE_BYTES = 12;
constexpr size_t TAR_TYPE_OFFSET = 156;
constexpr size_t TAR_MAGIC_OFFSET = 257;
constexpr size_t TAR_MAGIC_BYTES = 5;
constexpr char TAR_MAGIC[] = "ustar";
constexpr char OCTAL_FIRST_DIGIT = '0';
constexpr char TAR_REGULAR = OCTAL_FIRST_DIGIT;
constexpr char TAR_REGULAR_LEGACY = '\0';
constexpr char TAR_DIRECTORY = '5';
constexpr char PATH_SEPARATOR = '/';
constexpr char TERMINATOR = '\0';
constexpr char FIELD_PADDING = ' ';
constexpr uint32_t OCTAL_BASE = 8;
constexpr uint32_t MAX_ARCHIVE_BYTES = cock::abi::shm::MAX_OBJECT_BYTES;

constexpr size_t ROOT_ENTRY = 0;
constexpr size_t SEPARATOR_BYTES = 1;
constexpr size_t EMPTY = 0;
constexpr uint32_t START_POSITION = 0;
constexpr uint32_t NO_SIZE = 0;

bool parseOctal(const uint8_t *field, size_t length, uint32_t &value) {
	value = NO_SIZE;
	for (size_t idx = EMPTY; idx < length && field[idx] != TERMINATOR && field[idx] != FIELD_PADDING;
		 idx++) {
		if (field[idx] < OCTAL_FIRST_DIGIT || field[idx] >= OCTAL_FIRST_DIGIT + OCTAL_BASE)
			return false;
		value = value * OCTAL_BASE + static_cast<uint32_t>(field[idx] - OCTAL_FIRST_DIGIT);
		if (value > MAX_ARCHIVE_BYTES) return false;
	}
	return true;
}

void buildIndex() {
	entries[ROOT_ENTRY].type = FileType::DIRECTORY;
	uint32_t position = START_POSITION;
	while (entryCount < MAX_ENTRIES && position + TAR_BLOCK_BYTES <= MAX_ARCHIVE_BYTES) {
		const uint8_t *header = archive + position;
		if (header[TAR_NAME_OFFSET] == TAR_REGULAR_LEGACY) break;
		if (memcmp(header + TAR_MAGIC_OFFSET, TAR_MAGIC, TAR_MAGIC_BYTES) != 0) break;

		uint32_t size = NO_SIZE;
		if (!parseOctal(header + TAR_SIZE_OFFSET, TAR_SIZE_BYTES, size)) break;

		char type = static_cast<char>(header[TAR_TYPE_OFFSET]);
		bool regular = type == TAR_REGULAR || type == TAR_REGULAR_LEGACY;
		if (regular || type == TAR_DIRECTORY) {
			Entry &entry = entries[entryCount++];
			memset(&entry, 0, sizeof(entry));
			memcpy(entry.name, header, TAR_NAME_BYTES);
			size_t length = strlen(entry.name);
			while (length > EMPTY && entry.name[length - 1] == PATH_SEPARATOR)
				entry.name[--length] = TERMINATOR;
			entry.offset = position + TAR_BLOCK_BYTES;
			entry.size = regular ? size : NO_SIZE;
			entry.type = regular ? FileType::REGULAR : FileType::DIRECTORY;
		}

		uint32_t blocks = (size + TAR_BLOCK_BYTES - 1) / TAR_BLOCK_BYTES;
		position += TAR_BLOCK_BYTES + blocks * TAR_BLOCK_BYTES;
	}
}

} // namespace

const uint8_t *archive = nullptr;
Entry entries[MAX_ENTRIES];
size_t entryCount = FIRST_FILE_ENTRY;

bool loadArchive() {
	uint32_t module = cock::user::module_open(cock::abi::fs::INITRD_MODULE_NAME);
	if (module == cock::user::NO_SHM) return false;

	archive = static_cast<const uint8_t *>(cock::user::shm_map(module, false));
	if (archive == nullptr) return false;
	buildIndex();
	return true;
}

const Entry *findEntry(const char *path) {
	if (*path == PATH_SEPARATOR) path++;
	size_t length = strlen(path);
	while (length > EMPTY && path[length - 1] == PATH_SEPARATOR)
		length--;
	if (length > TAR_NAME_BYTES) return nullptr;
	if (length == EMPTY) return &entries[ROOT_ENTRY];

	for (size_t idx = FIRST_FILE_ENTRY; idx < entryCount; idx++) {
		if (strlen(entries[idx].name) == length && memcmp(entries[idx].name, path, length) == 0)
			return &entries[idx];
	}
	return nullptr;
}

bool isChild(const Entry &directory, const Entry &candidate, const char *&name) {
	name = candidate.name;
	size_t prefix = strlen(directory.name);
	if (prefix > EMPTY) {
		if (strlen(name) <= prefix + SEPARATOR_BYTES || memcmp(name, directory.name, prefix) != 0 ||
			name[prefix] != PATH_SEPARATOR)
			return false;
		name += prefix + SEPARATOR_BYTES;
	}
	for (const char *cursor = name; *cursor != TERMINATOR; cursor++)
		if (*cursor == PATH_SEPARATOR) return false;
	return true;
}

} // namespace cock::ramfs
