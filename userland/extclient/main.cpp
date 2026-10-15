#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using cock::abi::fs::FileType;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;

namespace {

constexpr char SMALL_PATH[] = "/ext/small.txt";
constexpr char SMALL_TEXT[] = "hello from ext2\n";
constexpr char INNER_PATH[] = "/ext/subdir/inner.txt";
constexpr char INNER_TEXT[] = "inner\n";
constexpr char LARGE_PATH[] = "/ext/large.bin";
constexpr char SPARSE_PATH[] = "/ext/sparse.bin";
constexpr char EMPTY_PATH[] = "/ext/empty.txt";
constexpr char ROOT_PATH[] = "/ext";
constexpr char MANY_PATH[] = "/ext/many";
constexpr char FAST_LINK_PATH[] = "/ext/link-fast";
constexpr char SLOW_LINK_PATH[] = "/ext/link-slow";
constexpr char ABSOLUTE_LINK_PATH[] = "/ext/link-abs";
constexpr char RELATIVE_LINK_PATH[] = "/ext/link-sub";
constexpr char LOOP_PATH[] = "/ext/loop-a";
constexpr char MISSING_PATH[] = "/ext/missing.txt";
constexpr char THROUGH_FILE_PATH[] = "/ext/small.txt/inside";
constexpr char FAST_LINK_NAME[] = "link-fast";
constexpr char SUBDIRECTORY_NAME[] = "subdir";
constexpr char LARGE_NAME[] = "large.bin";
constexpr char CURRENT_NAME[] = ".";
constexpr char PARENT_NAME[] = "..";
constexpr char FIRST_MANY_NAME[] = "file001";
constexpr char LAST_MANY_NAME[] = "file120";
constexpr char SPARSE_HEAD[] = "head";
constexpr char SPARSE_TAIL[] = "tail";
constexpr size_t SPARSE_HEAD_BYTES = sizeof(SPARSE_HEAD) - sizeof(char);
constexpr size_t SPARSE_TAIL_BYTES = sizeof(SPARSE_TAIL) - sizeof(char);
constexpr uint32_t SPARSE_BYTES = 50000;
constexpr uint32_t SPARSE_PROBE_OFFSET = 20000;
constexpr size_t SPARSE_PROBE_BYTES = 4096;
constexpr uint32_t LARGE_BYTES = 300000;
constexpr uint32_t PATTERN_LETTERS = 26;
constexpr char PATTERN_FIRST = 'A';
constexpr uint32_t DIRECT_BOUNDARY_OFFSET = 12280;
constexpr uint32_t DOUBLE_BOUNDARY_OFFSET = 274420;
constexpr size_t BOUNDARY_BYTES = 30;
constexpr uint32_t MANY_ENTRIES = 120;
constexpr size_t CHUNK_BYTES = 4096;
constexpr size_t TEXT_BYTES = 64;
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

bool readsLargeFile(Client &client) {
	uint32_t handle = client.open(LARGE_PATH);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	bool ok = client.stat(handle, info) && info.size == LARGE_BYTES;
	char chunk[CHUNK_BYTES];
	uint32_t position = START_OFFSET;
	while (ok && position < LARGE_BYTES) {
		long count = client.read(handle, position, chunk, sizeof(chunk));
		ok = count > 0;
		for (long index = 0; ok && index < count; index++)
			ok = chunk[index] == pattern(position + static_cast<uint32_t>(index));
		if (ok) position += static_cast<uint32_t>(count);
	}
	ok = ok && position == LARGE_BYTES;

	const uint32_t offsets[] = {DIRECT_BOUNDARY_OFFSET, DOUBLE_BOUNDARY_OFFSET};
	for (uint32_t offset : offsets) {
		long count = client.read(handle, offset, chunk, BOUNDARY_BYTES);
		ok = ok && count == static_cast<long>(BOUNDARY_BYTES);
		for (size_t index = 0; ok && index < BOUNDARY_BYTES; index++)
			ok = chunk[index] == pattern(offset + static_cast<uint32_t>(index));
	}
	return client.close(handle) && ok;
}

bool readsSparseFile(Client &client) {
	uint32_t handle = client.open(SPARSE_PATH);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	char chunk[CHUNK_BYTES];
	bool ok = client.stat(handle, info) && info.size == SPARSE_BYTES &&
			  client.read(handle, START_OFFSET, chunk, SPARSE_HEAD_BYTES) ==
				  static_cast<long>(SPARSE_HEAD_BYTES) &&
			  memcmp(chunk, SPARSE_HEAD, SPARSE_HEAD_BYTES) == 0;

	ok = ok && client.read(handle, SPARSE_PROBE_OFFSET, chunk, SPARSE_PROBE_BYTES) ==
				   static_cast<long>(SPARSE_PROBE_BYTES);
	for (size_t index = 0; ok && index < SPARSE_PROBE_BYTES; index++)
		ok = chunk[index] == '\0';

	ok = ok && client.read(handle, SPARSE_BYTES - SPARSE_TAIL_BYTES, chunk,
						   SPARSE_TAIL_BYTES) == static_cast<long>(SPARSE_TAIL_BYTES) &&
		 memcmp(chunk, SPARSE_TAIL, SPARSE_TAIL_BYTES) == 0;
	return client.close(handle) && ok;
}

bool listsTheVolume(Client &client) {
	uint32_t root = client.open(ROOT_PATH);
	if (root == cock::abi::fs::NO_HANDLE) return false;

	bool sawLink = false;
	bool sawDirectory = false;
	bool sawLarge = false;
	bool sawDots = false;
	uint32_t count = START_OFFSET;
	char name[TEXT_BYTES] = {};
	FileType type = FileType::REGULAR;
	while (client.readdir(root, count, name, sizeof(name), type)) {
		count++;
		if (strcmp(name, FAST_LINK_NAME) == 0) sawLink = type == FileType::SYMLINK;
		if (strcmp(name, SUBDIRECTORY_NAME) == 0) sawDirectory = type == FileType::DIRECTORY;
		if (strcmp(name, LARGE_NAME) == 0) sawLarge = type == FileType::REGULAR;
		if (strcmp(name, CURRENT_NAME) == 0 || strcmp(name, PARENT_NAME) == 0) sawDots = true;
	}
	return client.close(root) && sawLink && sawDirectory && sawLarge && !sawDots;
}

bool listsALargeDirectory(Client &client) {
	uint32_t directory = client.open(MANY_PATH);
	if (directory == cock::abi::fs::NO_HANDLE) return false;

	bool sawFirst = false;
	bool sawLast = false;
	uint32_t count = START_OFFSET;
	char name[TEXT_BYTES] = {};
	FileType type = FileType::REGULAR;
	while (client.readdir(directory, count, name, sizeof(name), type)) {
		count++;
		if (strcmp(name, FIRST_MANY_NAME) == 0) sawFirst = true;
		if (strcmp(name, LAST_MANY_NAME) == 0) sawLast = true;
	}
	return client.close(directory) && count == MANY_ENTRIES && sawFirst && sawLast;
}

bool followsLinks(Client &client) {
	return readsText(client, FAST_LINK_PATH, SMALL_TEXT, sizeof(SMALL_TEXT) - sizeof(char)) &&
		   readsText(client, SLOW_LINK_PATH, SMALL_TEXT, sizeof(SMALL_TEXT) - sizeof(char)) &&
		   readsText(client, ABSOLUTE_LINK_PATH, INNER_TEXT, sizeof(INNER_TEXT) - sizeof(char)) &&
		   readsText(client, RELATIVE_LINK_PATH, INNER_TEXT, sizeof(INNER_TEXT) - sizeof(char)) &&
		   client.open(LOOP_PATH) == cock::abi::fs::NO_HANDLE;
}

bool readsAnEmptyFile(Client &client) {
	uint32_t handle = client.open(EMPTY_PATH);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	char buffer[TEXT_BYTES];
	bool ok = client.stat(handle, info) && info.size == START_OFFSET &&
			  client.read(handle, START_OFFSET, buffer, sizeof(buffer)) == 0;
	return client.close(handle) && ok;
}

bool refusesBadPaths(Client &client) {
	return client.open(MISSING_PATH) == cock::abi::fs::NO_HANDLE &&
		   client.open(THROUGH_FILE_PATH) == cock::abi::fs::NO_HANDLE;
}

}

extern "C" void _start() {
	Client client;
	bool served = client.connect();

	served = served &&
			 readsText(client, SMALL_PATH, SMALL_TEXT, sizeof(SMALL_TEXT) - sizeof(char));
	if (served) printf("extclient: small says %s", SMALL_TEXT);

	served = served && readsText(client, INNER_PATH, INNER_TEXT, sizeof(INNER_TEXT) - sizeof(char));
	served = served && readsLargeFile(client);
	served = served && readsSparseFile(client);
	served = served && readsAnEmptyFile(client);
	served = served && listsTheVolume(client);
	served = served && listsALargeDirectory(client);
	served = served && followsLinks(client);
	served = served && refusesBadPaths(client);

	puts(served ? "extclient: ok" : "extclient: refused");
	cock::user::exit_process(served ? EXIT_SERVED : EXIT_REFUSED);
}
