#include <cock/abi/manifest.hpp>
#include <cock/abi/mounts.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using cock::abi::manifest::Entry;
using cock::abi::manifest::Manifest;
using cock::abi::mounts::Error;
using cock::abi::mounts::Table;

namespace {

constexpr char GOOD_TABLE[] =
	"# mounts\n"
	"/ ramfs\n"
	"\n"
	"  /fat   fat  \r\n"
	"/a/b deep";
constexpr size_t GOOD_COUNT = 3;
constexpr size_t ROOT_SLOT = 0;
constexpr size_t FAT_SLOT = 1;
constexpr size_t DEEP_SLOT = 2;
constexpr size_t FIRST_LINE = 1;
constexpr size_t SECOND_LINE = 2;
constexpr size_t MOUNT_LIMIT_LINES = 9;
constexpr size_t LONG_TEXT_BYTES = 128;
constexpr char LONG_FILL = 'a';
constexpr char ROOT_LINE[] = "/ ";
constexpr char ROOT_ENTRY[] = "/ ramfs\n";
constexpr char EXTRA_SERVER[] = " x\n";
constexpr char EXTRA_FIRST_LETTER = 'b';
constexpr char EXTRA_SEPARATOR = '/';
constexpr size_t EXTRA_START_LINE = 1;
constexpr char NEWLINE = '\n';
constexpr char ARGUMENT_MANIFEST[] =
	"fat path=bin/fat.elf arg=2\n"
	"plain path=bin/plain.elf\n"
	"hex path=bin/hex.elf arg=0x10\n";
constexpr uint32_t FAT_ARGUMENT = 2;
constexpr uint32_t HEX_ARGUMENT = 16;
constexpr size_t FAT_ENTRY = 0;
constexpr size_t PLAIN_ENTRY = 1;
constexpr size_t HEX_ENTRY = 2;

bool parses(const char *text, Table &table) { return table.parse(text, strlen(text)); }

Error errorOf(const char *text) {
	Table table;
	if (parses(text, table)) return Error::NONE;
	return table.error;
}

void checkGoodTable() {
	Table table;
	CHECK_TRUE(parses(GOOD_TABLE, table));
	CHECK_EQ(GOOD_COUNT, table.count);
	CHECK_TRUE(strcmp(table.mounts[ROOT_SLOT].prefix, "/") == 0);
	CHECK_TRUE(strcmp(table.mounts[ROOT_SLOT].server, "ramfs") == 0);
	CHECK_TRUE(strcmp(table.mounts[FAT_SLOT].prefix, "/fat") == 0);
	CHECK_TRUE(strcmp(table.mounts[FAT_SLOT].server, "fat") == 0);
	CHECK_TRUE(strcmp(table.mounts[DEEP_SLOT].prefix, "/a/b") == 0);
	CHECK_TRUE(table.find("/fat") == &table.mounts[FAT_SLOT]);
	CHECK_TRUE(table.find("/nothing") == nullptr);
}

void checkRejections() {
	CHECK_TRUE(errorOf("/fat fat\n") == Error::NO_ROOT);
	CHECK_TRUE(errorOf("") == Error::NO_ROOT);
	CHECK_TRUE(errorOf("/ ramfs\nfat fat\n") == Error::BAD_PREFIX);
	CHECK_TRUE(errorOf("/ ramfs\n/fat/ fat\n") == Error::BAD_PREFIX);
	CHECK_TRUE(errorOf("/ ramfs\n/a//b fat\n") == Error::BAD_PREFIX);
	CHECK_TRUE(errorOf("/ ramfs\n/fat\n") == Error::WRONG_FIELDS);
	CHECK_TRUE(errorOf("/ ramfs\n/fat fat extra\n") == Error::WRONG_FIELDS);
	CHECK_TRUE(errorOf("/ ramfs\n/ other\n") == Error::DUPLICATE_PREFIX);

	Table table;
	CHECK_FALSE(parses("/ ramfs\n/fat\n", table));
	CHECK_EQ(SECOND_LINE, table.errorLine);
	CHECK_FALSE(parses("bad\n", table));
	CHECK_EQ(FIRST_LINE, table.errorLine);

	char text[LONG_TEXT_BYTES + LONG_TEXT_BYTES];
	memset(text, LONG_FILL, sizeof(text));
	memcpy(text, ROOT_LINE, sizeof(ROOT_LINE) - sizeof(char));
	text[LONG_TEXT_BYTES] = NEWLINE;
	CHECK_FALSE(table.parse(text, LONG_TEXT_BYTES + 1));
	CHECK_TRUE(table.error == Error::BAD_SERVER);

	char many[MOUNT_LIMIT_LINES * LONG_TEXT_BYTES] = {};
	size_t length = 0;
	memcpy(many, ROOT_ENTRY, sizeof(ROOT_ENTRY) - sizeof(char));
	length = sizeof(ROOT_ENTRY) - sizeof(char);
	for (size_t line = EXTRA_START_LINE; line < MOUNT_LIMIT_LINES; line++) {
		many[length++] = EXTRA_SEPARATOR;
		many[length++] = static_cast<char>(EXTRA_FIRST_LETTER + line);
		memcpy(many + length, EXTRA_SERVER, sizeof(EXTRA_SERVER) - sizeof(char));
		length += sizeof(EXTRA_SERVER) - sizeof(char);
	}
	CHECK_FALSE(table.parse(many, length));
	CHECK_TRUE(table.error == Error::TOO_MANY_MOUNTS);
}

void checkManifestArgument() {
	Manifest manifest;
	CHECK_TRUE(manifest.parse(ARGUMENT_MANIFEST, strlen(ARGUMENT_MANIFEST)));
	CHECK_TRUE(manifest.entries[FAT_ENTRY].hasArgument);
	CHECK_EQ(FAT_ARGUMENT, manifest.entries[FAT_ENTRY].argument);
	CHECK_FALSE(manifest.entries[PLAIN_ENTRY].hasArgument);
	CHECK_TRUE(manifest.entries[HEX_ENTRY].hasArgument);
	CHECK_EQ(HEX_ARGUMENT, manifest.entries[HEX_ENTRY].argument);

	CHECK_FALSE(manifest.parse("x path=a arg=1 arg=2", strlen("x path=a arg=1 arg=2")));
	CHECK_TRUE(manifest.error == cock::abi::manifest::Error::DUPLICATE_KEY);
	CHECK_FALSE(manifest.parse("x path=a arg=zz", strlen("x path=a arg=zz")));
	CHECK_TRUE(manifest.error == cock::abi::manifest::Error::BAD_VALUE);
}

} // namespace

void testMounts() {
	checkGoodTable();
	checkRejections();
	checkManifestArgument();
}

} // namespace host_test
