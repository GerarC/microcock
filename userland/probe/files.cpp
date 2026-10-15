#include "probe.hpp"
#include <cock/abi/fs.hpp>
#include <cock/user/api.hpp>
#include <cock/user/fs.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using cock::abi::fs::Status;
using cock::ipc::MessageType;
using cock::user::fs::Client;
using cock::user::fs::FileInfo;
using cock::user::fs::Reply;

namespace probe {

namespace {

constexpr char INITRD_FILE[] = "hello.txt";
constexpr char INITRD_ROOTED_FILE[] = "/hello.txt";
constexpr char MISSING_FILE[] = "no-such-file";
constexpr char ROOT_DIRECTORY[] = "/";
constexpr char NESTED_DIRECTORY[] = "docs";
constexpr char NESTED_FILE[] = "docs/note.txt";
constexpr char NESTED_NAME[] = "note.txt";
constexpr char NESTED_TEXT[] = "Cucked file\n";
constexpr size_t NESTED_TEXT_BYTES = sizeof(NESTED_TEXT) - sizeof(char);
constexpr uint32_t ROOT_ENTRY_COUNT = 2;
constexpr char INITRD_TEXT[] = "Hello from the cock initrd!!!\n";
constexpr size_t INITRD_TEXT_BYTES = sizeof(INITRD_TEXT) - sizeof(char);
constexpr size_t READ_BUFFER_BYTES = 64;
constexpr size_t SKIPPED_BYTES = 6;
constexpr uint32_t UNKNOWN_HANDLE = 0x7FFF;
constexpr uint32_t UNSUPPORTED_FLAGS = 1;
constexpr uint32_t NO_OFFSET = 0;
constexpr uint32_t NO_LENGTH = 0;
constexpr uint32_t NO_VALUE = 0;

bool fails(Client &client, MessageType type, uint32_t first, uint32_t second,
		   uint32_t third, Status expected) {
	Reply reply = {};
	return client.call(type, first, second, third, reply) && reply.status == expected;
}

} // namespace

bool initrdFileReads() {
	Client client;
	if (!client.connect()) return false;

	uint32_t handle = client.open(INITRD_FILE);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	FileInfo info = {};
	bool ok = client.stat(handle, info) && info.size == INITRD_TEXT_BYTES &&
			  info.type == cock::abi::fs::FileType::REGULAR;

	char buffer[READ_BUFFER_BYTES] = {};
	long count = client.read(handle, NO_OFFSET, buffer, sizeof(buffer) - sizeof(char));
	ok = ok && count == static_cast<long>(INITRD_TEXT_BYTES) &&
		 memcmp(buffer, INITRD_TEXT, INITRD_TEXT_BYTES) == 0;

	char tail[READ_BUFFER_BYTES] = {};
	long rest = client.read(handle, SKIPPED_BYTES, tail, sizeof(tail) - sizeof(char));
	ok = ok && rest == static_cast<long>(INITRD_TEXT_BYTES - SKIPPED_BYTES) &&
		 memcmp(tail, INITRD_TEXT + SKIPPED_BYTES, INITRD_TEXT_BYTES - SKIPPED_BYTES) ==
			 0;

	ok = ok && client.read(handle, INITRD_TEXT_BYTES, tail, sizeof(tail)) == 0;
	ok = ok && client.close(handle);

	uint32_t rooted = client.open(INITRD_ROOTED_FILE);
	ok = ok && rooted != cock::abi::fs::NO_HANDLE && client.close(rooted);

	if (ok) printf("probe: initrd says %s", buffer);
	return ok;
}

bool fileServerRefusesBadRequests() {
	Client client;
	if (!client.connect()) return false;

	uint32_t handle = client.open(INITRD_FILE);
	if (handle == cock::abi::fs::NO_HANDLE) return false;

	char buffer[READ_BUFFER_BYTES] = {};
	bool ok = client.open(MISSING_FILE) == cock::abi::fs::NO_HANDLE &&
			  client.read(UNKNOWN_HANDLE, NO_OFFSET, buffer, sizeof(buffer)) ==
				  cock::user::fs::READ_FAILED &&
			  client.read(cock::abi::fs::NO_HANDLE, NO_OFFSET, buffer,
						  sizeof(buffer)) == cock::user::fs::READ_FAILED &&
			  client.read(handle, INITRD_TEXT_BYTES + 1, buffer, sizeof(buffer)) ==
				  cock::user::fs::READ_FAILED;

	ok = ok && fails(client, MessageType::FS_OPEN, NO_LENGTH,
					 cock::abi::fs::OPEN_READ_ONLY, NO_VALUE, Status::BAD_REQUEST);
	ok = ok && fails(client, MessageType::FS_OPEN, cock::abi::fs::MAX_PATH_BYTES,
					 cock::abi::fs::OPEN_READ_ONLY, NO_VALUE, Status::BAD_REQUEST);
	ok = ok && fails(client, MessageType::FS_OPEN,
					 static_cast<uint32_t>(sizeof(INITRD_FILE) - sizeof(char)),
					 UNSUPPORTED_FLAGS, NO_VALUE, Status::UNSUPPORTED);
	ok = ok && fails(client, MessageType::FS_STAT, UNKNOWN_HANDLE, NO_VALUE, NO_VALUE,
					 Status::BAD_HANDLE);
	ok = ok && fails(client, MessageType::FS_CLOSE, UNKNOWN_HANDLE, NO_VALUE, NO_VALUE,
					 Status::BAD_HANDLE);

	ok = ok && client.close(handle) && !client.close(handle) &&
		 client.read(handle, NO_OFFSET, buffer, sizeof(buffer)) ==
			 cock::user::fs::READ_FAILED;
	return ok;
}

bool fileServerListsDirectories() {
	Client client;
	if (!client.connect()) return false;

	uint32_t root = client.open(ROOT_DIRECTORY);
	if (root == cock::abi::fs::NO_HANDLE) return false;

	bool sawFile = false;
	bool sawDirectory = false;
	uint32_t count = 0;
	char name[READ_BUFFER_BYTES] = {};
	cock::abi::fs::FileType type = cock::abi::fs::FileType::REGULAR;
	while (client.readdir(root, count, name, sizeof(name), type)) {
		count++;
		if (strcmp(name, INITRD_FILE) == 0)
			sawFile = type == cock::abi::fs::FileType::REGULAR;
		if (strcmp(name, NESTED_DIRECTORY) == 0)
			sawDirectory = type == cock::abi::fs::FileType::DIRECTORY;
	}
	bool ok = count == ROOT_ENTRY_COUNT && sawFile && sawDirectory;

	uint32_t nested = client.open(NESTED_DIRECTORY);
	ok = ok && nested != cock::abi::fs::NO_HANDLE &&
		 client.readdir(nested, NO_OFFSET, name, sizeof(name), type) &&
		 strcmp(name, NESTED_NAME) == 0 &&
		 !client.readdir(nested, 1, name, sizeof(name), type) && client.close(nested);

	uint32_t file = client.open(NESTED_FILE);
	char text[READ_BUFFER_BYTES] = {};
	ok = ok && file != cock::abi::fs::NO_HANDLE &&
		 client.read(file, NO_OFFSET, text, sizeof(text)) ==
			 static_cast<long>(NESTED_TEXT_BYTES) &&
		 memcmp(text, NESTED_TEXT, NESTED_TEXT_BYTES) == 0 &&
		 !client.readdir(file, NO_OFFSET, name, sizeof(name), type) &&
		 client.close(file);

	return ok && !client.readdir(UNKNOWN_HANDLE, NO_OFFSET, name, sizeof(name), type) &&
		   client.close(root);
}

bool moduleIndexIsDenied() {
	return cock::user::module_index(cock::abi::fs::INITRD_MODULE_NAME) ==
			   cock::user::NO_MODULE &&
		   syscall(SYS_MODULE_INDEX, KERNEL_ADDRESS, PROBE_LENGTH, 0, 0, 0, 0) ==
			   cock::user::FAILURE &&
		   syscall(SYS_MODULE_INDEX, UNMAPPED_USER_ADDRESS, PROBE_LENGTH, 0, 0, 0,
				   0) == cock::user::FAILURE;
}

} // namespace probe
