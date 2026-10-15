#ifndef COCK_USER_FS_HPP
#define COCK_USER_FS_HPP

#include <cock/abi/fs.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::user::fs {

using abi::fs::FileType;
using abi::fs::Status;
using ipc::Message;
using ipc::MessageType;

constexpr long READ_FAILED = -1;

struct Reply {
	Status status;
	uint32_t first;
	uint32_t second;
};

struct FileInfo {
	uint32_t size;
	FileType type;
};

class Client {
  public:
	void (*onForeign)(const Message &) = nullptr;

	bool connect(const char *name = abi::fs::SERVER_NAME) {
		server = lookup(name);
		if (server == NO_PROCESS) return false;

		Reply reply = {};
		if (!call(MessageType::FS_CONNECT, 0, 0, 0, reply) ||
			reply.status != Status::OK)
			return false;

		view = static_cast<uint8_t *>(shm_map(reply.first, true));
		return view != nullptr;
	}

	Status lastStatus() const { return status; }

	bool call(MessageType type, uint32_t first, uint32_t second, uint32_t third,
			  Reply &reply) {
		status = Status::NOT_CONNECTED;
		if (server == NO_PROCESS) return false;

		uint32_t id = nextId++;
		Message request = {};
		request.type = type;
		request.payload.args.arg0 = id;
		request.payload.args.arg1 = first;
		request.payload.args.arg2 = second;
		request.payload.args.arg3 = third;
		if (!ipc_send(server, request)) return false;

		Message answer = {};
		for (;;) {
			if (!ipc_recv(answer)) return false;
			if (answer.type == MessageType::FS_REPLY && answer.sender_pid == server &&
				answer.payload.args.arg0 == id)
				break;
			if (onForeign != nullptr) onForeign(answer);
		}

		reply.status = static_cast<Status>(answer.payload.args.arg1);
		status = reply.status;
		reply.first = answer.payload.args.arg2;
		reply.second = answer.payload.args.arg3;
		return true;
	}

	uint32_t open(const char *path) {
		size_t length = strlen(path);
		if (view == nullptr || length == 0 || length >= abi::fs::MAX_PATH_BYTES)
			return abi::fs::NO_HANDLE;

		memcpy(view, path, length);
		view[length] = '\0';

		Reply reply = {};
		if (!call(MessageType::FS_OPEN, static_cast<uint32_t>(length),
				  abi::fs::OPEN_READ_ONLY, 0, reply) ||
			reply.status != Status::OK)
			return abi::fs::NO_HANDLE;
		return reply.first;
	}

	long read(uint32_t handle, uint32_t offset, void *buffer, size_t length) {
		if (view == nullptr) return READ_FAILED;
		if (length > abi::fs::WINDOW_BYTES) length = abi::fs::WINDOW_BYTES;

		Reply reply = {};
		if (!call(MessageType::FS_READ, handle, offset, static_cast<uint32_t>(length),
				  reply) ||
			reply.status != Status::OK || reply.first > length)
			return READ_FAILED;

		memcpy(buffer, view, reply.first);
		return static_cast<long>(reply.first);
	}

	bool readdir(uint32_t handle, uint32_t index, char *name, size_t size,
				 FileType &type) {
		Reply reply = {};
		if (view == nullptr || size == 0 ||
			!call(MessageType::FS_READDIR, handle, index, 0, reply) ||
			reply.status != Status::OK || reply.first >= size ||
			reply.first >= abi::fs::WINDOW_BYTES)
			return false;

		memcpy(name, view, reply.first + sizeof(char));
		type = static_cast<FileType>(reply.second);
		return true;
	}

	bool stat(uint32_t handle, FileInfo &info) {
		Reply reply = {};
		if (!call(MessageType::FS_STAT, handle, 0, 0, reply) ||
			reply.status != Status::OK)
			return false;

		info.size = reply.first;
		info.type = static_cast<FileType>(reply.second);
		return true;
	}

	bool close(uint32_t handle) {
		Reply reply = {};
		return call(MessageType::FS_CLOSE, handle, 0, 0, reply) &&
			   reply.status == Status::OK;
	}

  private:
	uint32_t server = NO_PROCESS;
	uint8_t *view = nullptr;
	uint32_t nextId = 1;
	Status status = Status::NOT_CONNECTED;
};

} // namespace cock::user::fs

#endif // !COCK_USER_FS_HPP
