/**
 * @file block.hpp
 * @brief Client of the block storage server (the ATA driver) in Ring 3.
 *
 * Requests travel as asynchronous messages (`BLK_*` types) sent through the handle the name
 * server gave, asking for a reply token; the server answers through that token. Bulk data travels
 * through a shared memory window that the server creates on `BLK_CONNECT`, shared by process id.
 * Every request carries an id and the matching `BLK_REPLY` echoes it. All calls block until the
 * reply arrives or the server dies.
 */
#ifndef COCK_USER_BLOCK_HPP
#define COCK_USER_BLOCK_HPP

#include <cock/abi/block.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <cock/user/reply.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::user::block {

using abi::block::DeviceKind;
using abi::block::Status;
using ipc::Message;
using ipc::MessageType;

/** Decoded `BLK_REPLY`: the status of the server and up to three result words. */
struct Reply {
	/** Result code given by the server. */
	Status status;
	/** First result word; its meaning depends on the request. */
	uint32_t first;
	/** Second result word. */
	uint32_t second;
	/** Third result word. */
	uint32_t third;
};

/** Description of one block device, as answered to `BLK_INFO`. */
struct DeviceInfo {
	/** Number of sectors of the device. */
	uint32_t sectorCount;
	/** First sector of the device (zero for a whole disk). */
	uint32_t firstSector;
	/** Kind of device (whole disk or partition). */
	DeviceKind kind;
};

/**
 * @brief Connection to the block server.
 *
 * Not thread safe: one request is in flight at a time and all state is per object.
 */
class Client {
  public:
	/**
	 * Called with every message that arrives while a reply is awaited and is not that
	 * reply. When null such messages are dropped.
	 */
	void (*onForeign)(const Message &) = nullptr;

	/**
	 * @brief Finds the block server, asks for a session and maps the shared window.
	 *
	 * Connects to the server through the name server, sends `BLK_CONNECT` and maps the
	 * shared object that comes back, writable. Blocks until the server answers.
	 *
	 * After a failure the client is marked lost: the next `info`, `read` or `write` calls
	 * `connect` again, so a block server that was started again is found by its name and
	 * gets a new session. The mapping of the old window is not released.
	 * @return true when connected; false when the server does not exist, refused the
	 *         session, died, or the window could not be mapped.
	 */
	bool connect() {
		lost = false;
		names::Connection connection = names::connect(abi::block::SERVER_NAME);
		server = connection.pid;
		handle = connection.handle;
		if (server == NO_PROCESS || handle == abi::NO_CAPABILITY_NODE) {
			lost = true;
			return false;
		}

		Reply reply = {};
		if (!call(MessageType::BLK_CONNECT, 0, 0, 0, reply) ||
			reply.status != Status::OK) {
			lost = true;
			return false;
		}

		view = static_cast<uint8_t *>(shm_map(reply.first, true));
		devices = reply.second;
		return view != nullptr;
	}

	/**
	 * @brief Returns the number of devices the server reported on `connect`.
	 * @return Device count; zero before a successful `connect`.
	 */
	uint32_t deviceCount() const { return devices; }

	/**
	 * @brief Asks for the description of a device (`BLK_INFO`). Blocks for the reply.
	 * @param device Device index, below `deviceCount()`.
	 * @param found Receives sector count, first sector and kind on success.
	 * @return true on success; false when the index is unknown, the server is not
	 *         connected or died.
	 */
	bool info(uint32_t device, DeviceInfo &found) {
		Reply reply = {};
		if (!attached() || !call(MessageType::BLK_INFO, device, 0, 0, reply) ||
			reply.status != Status::OK)
			return false;

		found.sectorCount = reply.first;
		found.firstSector = reply.second;
		found.kind = static_cast<DeviceKind>(reply.third);
		return true;
	}

	/**
	 * @brief Reads sectors (`BLK_READ`). Blocks until the driver finished the transfer.
	 *
	 * The server fills the shared window and the data is copied to the caller.
	 * @param device Device index.
	 * @param sector First sector, relative to the device.
	 * @param count Number of sectors, from 1 to `MAX_SECTORS_PER_READ`.
	 * @param buffer Destination of at least `count * SECTOR_BYTES` bytes.
	 * @return true when all sectors were read; false when not connected, the count is
	 *         out of range, the server reported an error or a short count, or it died.
	 */
	bool read(uint32_t device, uint32_t sector, uint32_t count, void *buffer) {
		if (!attached() || count == 0 || count > abi::block::MAX_SECTORS_PER_READ)
			return false;

		Reply reply = {};
		if (!call(MessageType::BLK_READ, device, sector, count, reply) ||
			reply.status != Status::OK || reply.first != count)
			return false;

		memcpy(buffer, view, size_t(count) * abi::block::SECTOR_BYTES);
		return true;
	}

	/**
	 * @brief Writes sectors (`BLK_WRITE`). Blocks until the driver finished the transfer.
	 *
	 * The data is copied to the shared window before the request is sent.
	 * @param device Device index.
	 * @param sector First sector, relative to the device.
	 * @param count Number of sectors, from 1 to `MAX_SECTORS_PER_WRITE`.
	 * @param buffer Source of at least `count * SECTOR_BYTES` bytes.
	 * @return true when all sectors were written; false when not connected, the count is
	 *         out of range, the server reported an error or a short count, or it died.
	 */
	bool write(uint32_t device, uint32_t sector, uint32_t count, const void *buffer) {
		if (!attached() || count == 0 || count > abi::block::MAX_SECTORS_PER_WRITE)
			return false;

		memcpy(view, buffer, size_t(count) * abi::block::SECTOR_BYTES);
		Reply reply = {};
		if (!call(MessageType::BLK_WRITE, device, sector, count, reply) ||
			reply.status != Status::OK || reply.first != count)
			return false;
		return true;
	}

  private:
	/**
	 * @brief Tells whether a window is mapped, connecting again first when the server was lost.
	 * @return true when the client is connected to a live server; false when it never was or
	 *         the server cannot be reached yet.
	 */
	bool attached() {
		if (lost && !connect()) return false;
		return view != nullptr;
	}

	/**
	 * @brief Sends one request and waits for the reply with the same id.
	 * @param type Request message type.
	 * @param first Request argument 1.
	 * @param second Request argument 2.
	 * @param third Request argument 3.
	 * @param reply Receives the decoded reply.
	 * @return true when a matching reply arrived; false when the server is unknown, the
	 *         send failed or the server died while waiting.
	 */
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
		if (!ep_send(handle, request, true)) {
			lost = !process_alive(server);
			return false;
		}

		Message answer = {};
		bool matches = receiveReply(
			server, answer,
			[&](const Message &candidate) {
				return candidate.type == MessageType::BLK_REPLY &&
					   candidate.badge == abi::endpoint::NO_BADGE &&
					   candidate.payload.args.arg0 == id;
			},
			onForeign);
		if (!matches) {
			lost = true;
			return false;
		}

		reply.status = static_cast<Status>(answer.payload.args.arg1);
		reply.first = answer.payload.args.arg2;
		reply.second = answer.payload.args.arg3;
		reply.third = answer.payload.args.arg4;
		return true;
	}

	/** Process id of the block server, `NO_PROCESS` before `connect`; used to see whether it is alive. */
	uint32_t server = NO_PROCESS;
	/** Handle on the endpoint of the block server, `NO_CAPABILITY_NODE` before `connect`. */
	uint32_t handle = abi::NO_CAPABILITY_NODE;
	/** Mapping of the shared data window, null before `connect`. */
	uint8_t *view = nullptr;
	/** Device count reported on `connect`. */
	uint32_t devices = 0;
	/** Id of the next request. */
	uint32_t nextId = 1;
	/** True when the last attempt to reach the server failed because it was gone. */
	bool lost = false;
};

} // namespace cock::user::block

#endif // !COCK_USER_BLOCK_HPP
