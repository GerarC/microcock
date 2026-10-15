#ifndef COCK_USER_BLOCK_HPP
#define COCK_USER_BLOCK_HPP

#include <cock/abi/block.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::user::block {

using abi::block::DeviceKind;
using abi::block::Status;
using ipc::Message;
using ipc::MessageType;

struct Reply {
	Status status;
	uint32_t first;
	uint32_t second;
	uint32_t third;
};

struct DeviceInfo {
	uint32_t sectorCount;
	uint32_t firstSector;
	DeviceKind kind;
};

class Client {
  public:
	void (*onForeign)(const Message &) = nullptr;

	bool connect() {
		server = lookup(abi::block::SERVER_NAME);
		if (server == NO_PROCESS) return false;

		Reply reply = {};
		if (!call(MessageType::BLK_CONNECT, 0, 0, 0, reply) ||
			reply.status != Status::OK)
			return false;

		view = static_cast<uint8_t *>(shm_map(reply.first, true));
		devices = reply.second;
		return view != nullptr;
	}

	uint32_t deviceCount() const { return devices; }

	bool info(uint32_t device, DeviceInfo &found) {
		Reply reply = {};
		if (!call(MessageType::BLK_INFO, device, 0, 0, reply) ||
			reply.status != Status::OK)
			return false;

		found.sectorCount = reply.first;
		found.firstSector = reply.second;
		found.kind = static_cast<DeviceKind>(reply.third);
		return true;
	}

	bool read(uint32_t device, uint32_t sector, uint32_t count, void *buffer) {
		if (view == nullptr || count == 0 || count > abi::block::MAX_SECTORS_PER_READ)
			return false;

		Reply reply = {};
		if (!call(MessageType::BLK_READ, device, sector, count, reply) ||
			reply.status != Status::OK || reply.first != count)
			return false;

		memcpy(buffer, view, size_t(count) * abi::block::SECTOR_BYTES);
		return true;
	}

  private:
	bool call(MessageType type, uint32_t first, uint32_t second, uint32_t third,
			  Reply &reply) {
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
			if (answer.type == MessageType::BLK_REPLY && answer.sender_pid == server &&
				answer.payload.args.arg0 == id)
				break;
			if (onForeign != nullptr) onForeign(answer);
		}

		reply.status = static_cast<Status>(answer.payload.args.arg1);
		reply.first = answer.payload.args.arg2;
		reply.second = answer.payload.args.arg3;
		reply.third = answer.payload.args.arg4;
		return true;
	}

	uint32_t server = NO_PROCESS;
	uint8_t *view = nullptr;
	uint32_t devices = 0;
	uint32_t nextId = 1;
};

} // namespace cock::user::block

#endif // !COCK_USER_BLOCK_HPP
