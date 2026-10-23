#include <cock/abi/console.hpp>
#include <console/display.hpp>
#include <console/input.hpp>
#include <console/readers.hpp>
#include <console/requests.hpp>
#include <console/writers.hpp>

using cock::abi::console::Op;
using cock::abi::console::Status;

namespace proto = cock::abi::console;

namespace cock::console {

namespace {

constexpr size_t NO_LENGTH = 0;
constexpr uint32_t CLEARED_WORD = 0;

} // namespace

Served serve(uint32_t sender, const uint32_t (&request)[ANSWER_WORDS],
			 uint32_t (&reply)[ANSWER_WORDS]) {
	for (uint32_t &word : reply)
		word = CLEARED_WORD;

	uint32_t header = request[proto::HEADER_WORD];
	size_t length = proto::lengthOf(header);
	char bytes[proto::CHUNK_BYTES] = {};

	switch (static_cast<Op>(proto::codeOf(header))) {
	case Op::WRITE:
		if (length == NO_LENGTH || length > proto::CHUNK_BYTES) {
			answer(reply, Status::BAD_REQUEST, NO_LENGTH);
			return Served::REPLY;
		}
		proto::unpackBytes(request, bytes, length);
		handleWrite(sender, bytes, length, proto::isLast(header));
		answer(reply, Status::OK, length);
		return Served::REPLY;
	case Op::CLEAR:
		screenClear();
		answer(reply, Status::OK, NO_LENGTH);
		return Served::REPLY;
	case Op::READ:
		if (length == NO_LENGTH || length > proto::CHUNK_BYTES) {
			answer(reply, Status::BAD_REQUEST, NO_LENGTH);
			return Served::REPLY;
		}
		if (discipline.hasInput()) {
			fillRead(reply, length);
			return Served::REPLY;
		}
		if (deferRead(length)) return Served::DEFERRED;
		answer(reply, Status::BUSY, NO_LENGTH);
		return Served::REPLY;
	}
	answer(reply, Status::BAD_REQUEST, NO_LENGTH);
	return Served::REPLY;
}

} // namespace cock::console
