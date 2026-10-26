#include <cock/abi/console.hpp>
#include <tty/readers.hpp>
#include <tty/requests.hpp>

using cock::abi::console::Op;
using cock::abi::console::Status;

namespace proto = cock::abi::console;

namespace cock::tty {

namespace {

constexpr size_t NO_LENGTH = 0;
constexpr uint32_t CLEARED_WORD = 0;

} // namespace

Served serve(const uint32_t (&request)[ANSWER_WORDS], uint32_t (&reply)[ANSWER_WORDS]) {
	for (uint32_t &word : reply)
		word = CLEARED_WORD;

	uint32_t header = request[proto::HEADER_WORD];
	size_t length = proto::lengthOf(header);
	if (static_cast<Op>(proto::codeOf(header)) != Op::READ || length == NO_LENGTH || length > proto::CHUNK_BYTES) {
		answer(reply, Status::BAD_REQUEST, NO_LENGTH);
		return Served::REPLY;
	}

	if (inputReady()) {
		fillRead(reply, length);
		return Served::REPLY;
	}
	if (deferRead(length)) return Served::DEFERRED;
	answer(reply, Status::BUSY, NO_LENGTH);
	return Served::REPLY;
}

} // namespace cock::tty
