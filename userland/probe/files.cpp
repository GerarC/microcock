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
constexpr char INITRD_TEXT[] = "Hello from the initrd\n";
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

bool moduleIndexIsDenied() {
	return cock::user::module_index(cock::abi::fs::INITRD_MODULE_NAME) ==
			   cock::user::NO_MODULE &&
		   syscall(SYS_MODULE_INDEX, KERNEL_ADDRESS, PROBE_LENGTH, 0, 0, 0, 0) ==
			   cock::user::FAILURE &&
		   syscall(SYS_MODULE_INDEX, UNMAPPED_USER_ADDRESS, PROBE_LENGTH, 0, 0, 0,
				   0) == cock::user::FAILURE;
}

} // namespace probe
