#include <ata/devices.hpp>
#include <ata/reply.hpp>
#include <ata/requests.hpp>
#include <cock/abi/block.hpp>

using cock::abi::block::Status;
using cock::ipc::Message;

namespace cock::ata {

void answerInfo(const Message &message) {
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;
	uint32_t device = message.payload.args.arg1;
	if (device >= deviceCount) return reply(token, id, Status::BAD_DEVICE);

	const Device &found = devices[device];
	reply(token, id, Status::OK, found.sectorCount, found.firstSector,
		  static_cast<uint32_t>(found.kind));
}

void answerRead(Session &session, const Message &message) {
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;
	uint32_t device = message.payload.args.arg1;
	uint32_t sector = message.payload.args.arg2;
	uint32_t count = message.payload.args.arg3;
	if (device >= deviceCount) return reply(token, id, Status::BAD_DEVICE);
	if (count == 0 || count > cock::abi::block::MAX_SECTORS_PER_READ)
		return reply(token, id, Status::BAD_REQUEST);

	const Device &found = devices[device];
	if (sector >= found.sectorCount || count > found.sectorCount - sector)
		return reply(token, id, Status::OUT_OF_RANGE);

	if (!drive.read(found.firstSector + sector, count, session.window))
		return reply(token, id, Status::IO_ERROR);
	reply(token, id, Status::OK, count);
}

void answerWrite(Session &session, const Message &message) {
	uint32_t token = message.reply;
	uint32_t id = message.payload.args.arg0;
	uint32_t device = message.payload.args.arg1;
	uint32_t sector = message.payload.args.arg2;
	uint32_t count = message.payload.args.arg3;
	if (device >= deviceCount) return reply(token, id, Status::BAD_DEVICE);
	if (device == cock::abi::block::DISK_DEVICE) return reply(token, id, Status::READ_ONLY);
	if (count == 0 || count > cock::abi::block::MAX_SECTORS_PER_WRITE)
		return reply(token, id, Status::BAD_REQUEST);

	const Device &found = devices[device];
	if (sector >= found.sectorCount || count > found.sectorCount - sector)
		return reply(token, id, Status::OUT_OF_RANGE);

	if (!drive.write(found.firstSector + sector, count, session.window))
		return reply(token, id, Status::IO_ERROR);
	reply(token, id, Status::OK, count);
}

} // namespace cock::ata
