#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/write_check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using cock::abi::fs::FileType;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

constexpr char ROOT_FILE[] = "/hello.txt";
constexpr char FAT_ROOT[] = "/fat";
constexpr char HELLO_PATH[] = "/fat/HELLO.TXT";
constexpr char LONG_PATH[] = "/fat/a long file name.txt";
constexpr char LONG_NAME[] = "A long file name.txt";
constexpr char UNICODE_PATH[] = "/fat/caf\xC3\xA9 \xC3\xB1.txt";
constexpr char UNICODE_NAME[] = "caf\xC3\xA9 \xC3\xB1.txt";
constexpr char ROOT_DIRECTORY[] = "/";
constexpr char MOUNT_NAME[] = "fat";
constexpr char ROOT_FILE_NAME[] = "hello.txt";
constexpr char NESTED_PATH[] = "/fat/subdir/nested.txt";
constexpr char BIG_PATH[] = "/fat/BIG.BIN";
constexpr char MISSING_PATH[] = "/fat/missing.txt";
constexpr char BEYOND_FILE_PATH[] = "/fat/HELLO.TXT/inside";
constexpr char HELLO_NAME[] = "HELLO.TXT";
constexpr char BIG_NAME[] = "BIG.BIN";
constexpr char DIRECTORY_NAME[] = "SUBDIR";
constexpr char HELLO_TEXT[] = "hello from fat\n";
constexpr char NESTED_TEXT[] = "nested\n";
constexpr size_t HELLO_BYTES = sizeof(HELLO_TEXT) - sizeof(char);
constexpr size_t NESTED_BYTES = sizeof(NESTED_TEXT) - sizeof(char);
constexpr uint32_t BIG_BYTES = 20000;
constexpr uint32_t PATTERN_LETTERS = 26;
constexpr char PATTERN_FIRST = 'A';
constexpr uint32_t UNALIGNED_OFFSET = 4090;
constexpr size_t UNALIGNED_BYTES = 20;
constexpr size_t CHUNK_BYTES = 4096;
constexpr size_t TEXT_BYTES = 64;
constexpr uint32_t LISTED_ENTRIES = 6;
constexpr char SCRATCH_PATH[] = "/fat/SCRATCH";
constexpr char MIXED_CASE_PATH[] = "/fat/SCRATCH/MixedCase.txt";
constexpr char LONG_NEW_PATH[] = "/fat/SCRATCH/a new long name.txt";
constexpr uint32_t WRITTEN_BIG_BYTES = 40000;
constexpr uint32_t START_OFFSET = 0;
constexpr uintptr_t EXIT_SERVED = 0;
constexpr uintptr_t EXIT_REFUSED = 1;

char pattern(uint32_t position) {
	return static_cast<char>(PATTERN_FIRST + position % PATTERN_LETTERS);
}

bool readsText(Client &client, const char *path, const char *text, size_t length) {
	uint32_t handle = client.open(path);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	char buffer[TEXT_BYTES] = {};
	FileInfo info = {};
	bool ok = client.stat(handle, info) && info.size == length &&
			  info.type == FileType::REGULAR &&
			  client.read(handle, START_OFFSET, buffer, sizeof(buffer)) ==
				  static_cast<long>(length) &&
			  memcmp(buffer, text, length) == 0;
	return client.close(handle) && ok;
}

bool readsBigFile(Client &client) {
	uint32_t handle = client.open(BIG_PATH);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	bool ok = client.stat(handle, info) && info.size == BIG_BYTES;
	char chunk[CHUNK_BYTES];
	uint32_t position = START_OFFSET;
	while (ok && position < BIG_BYTES) {
		long count = client.read(handle, position, chunk, sizeof(chunk));
		ok = count > 0;
		for (long index = 0; ok && index < count; index++)
			ok = chunk[index] == pattern(position + static_cast<uint32_t>(index));
		if (ok) position += static_cast<uint32_t>(count);
	}
	ok = ok && position == BIG_BYTES;

	long count = client.read(handle, UNALIGNED_OFFSET, chunk, UNALIGNED_BYTES);
	ok = ok && count == static_cast<long>(UNALIGNED_BYTES);
	for (size_t index = 0; ok && index < UNALIGNED_BYTES; index++)
		ok = chunk[index] == pattern(UNALIGNED_OFFSET + static_cast<uint32_t>(index));

	ok = ok && client.read(handle, BIG_BYTES, chunk, sizeof(chunk)) == 0 &&
		 client.read(handle, BIG_BYTES + 1, chunk, sizeof(chunk)) ==
			 cock::user::fs::READ_FAILED;
	return client.close(handle) && ok;
}

bool listsTheVolume(Client &client) {
	uint32_t root = client.open(FAT_ROOT);
	if (root == cock::abi::fs::NO_HANDLE) return false;

	bool sawHello = false;
	bool sawLong = false;
	bool sawBig = false;
	bool sawDirectory = false;
	bool sawUnicode = false;
	uint32_t count = START_OFFSET;
	char name[TEXT_BYTES] = {};
	FileType type = FileType::REGULAR;
	while (client.readdir(root, count, name, sizeof(name), type)) {
		count++;
		if (strcmp(name, HELLO_NAME) == 0) sawHello = type == FileType::REGULAR;
		if (strcmp(name, LONG_NAME) == 0) sawLong = type == FileType::REGULAR;
		if (strcmp(name, BIG_NAME) == 0) sawBig = type == FileType::REGULAR;
		if (strcmp(name, DIRECTORY_NAME) == 0) sawDirectory = type == FileType::DIRECTORY;
		if (strcmp(name, UNICODE_NAME) == 0) sawUnicode = type == FileType::REGULAR;
	}
	return client.close(root) && count == LISTED_ENTRIES && sawHello && sawLong &&
		   sawBig && sawDirectory && sawUnicode;
}

bool rootShowsTheMounts(Client &client) {
	uint32_t root = client.open(ROOT_DIRECTORY);
	if (root == cock::abi::fs::NO_HANDLE) return false;

	bool sawFile = false;
	bool sawMount = false;
	uint32_t count = START_OFFSET;
	char name[TEXT_BYTES] = {};
	FileType type = FileType::REGULAR;
	while (client.readdir(root, count, name, sizeof(name), type)) {
		count++;
		if (strcmp(name, ROOT_FILE_NAME) == 0) sawFile = type == FileType::REGULAR;
		if (strcmp(name, MOUNT_NAME) == 0) sawMount = type == FileType::DIRECTORY;
	}
	return client.close(root) && sawFile && sawMount;
}

bool refusesBadPaths(Client &client) {
	return client.open(MISSING_PATH) == cock::abi::fs::NO_HANDLE &&
		   client.open(BEYOND_FILE_PATH) == cock::abi::fs::NO_HANDLE;
}

bool writesTheVolume(Client &client) {
	cock::user::fs::WriteCheck check(client, SCRATCH_PATH, WRITTEN_BIG_BYTES);
	return check.run() &&
		   check.refuses(MIXED_CASE_PATH,
						 cock::abi::fs::OPEN_WRITE | cock::abi::fs::OPEN_CREATE,
						 cock::abi::fs::Status::UNSUPPORTED) &&
		   check.refuses(LONG_NEW_PATH, cock::abi::fs::OPEN_WRITE | cock::abi::fs::OPEN_CREATE,
						 cock::abi::fs::Status::UNSUPPORTED);
}

}

extern "C" void _start() {
	Client client;
	bool served = client.connect();

	served = served && readsText(client, HELLO_PATH, HELLO_TEXT, HELLO_BYTES);
	if (served) printf("fatclient: hello says %s", HELLO_TEXT);

	served = served && readsText(client, LONG_PATH, HELLO_TEXT, HELLO_BYTES);
	served = served && readsText(client, NESTED_PATH, NESTED_TEXT, NESTED_BYTES);
	served = served && readsText(client, UNICODE_PATH, HELLO_TEXT, HELLO_BYTES);
	served = served && readsBigFile(client);
	served = served && listsTheVolume(client);
	served = served && rootShowsTheMounts(client);
	served = served && refusesBadPaths(client);
	served = served && client.open(ROOT_FILE) != cock::abi::fs::NO_HANDLE;
	served = served && writesTheVolume(client);

	puts(served ? "fatclient: ok" : "fatclient: refused");
	cock::user::exit_process(served ? EXIT_SERVED : EXIT_REFUSED);
}
