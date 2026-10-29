#include <cock/abi/console.hpp>
#include <console/display.hpp>
#include <console/requests.hpp>
#include <console/tty_link.hpp>
#include <console/writers.hpp>

using cock::abi::console::Op;
using cock::abi::console::Status;

namespace proto = cock::abi::console;

namespace cock::console {

namespace {

constexpr size_t NO_LENGTH = 0;
constexpr uint32_t CLEARED_WORD = 0;

} // namespace

void serve(uint32_t sender, const uint32_t (&request)[ANSWER_WORDS], uint32_t (&reply)[ANSWER_WORDS]) {
	for (uint32_t &word : reply)
		word = CLEARED_WORD;

	uint32_t header = request[proto::HEADER_WORD];
	size_t length = proto::lengthOf(header);
	uint32_t terminal = proto::terminalOf(header);
	char bytes[proto::CHUNK_BYTES] = {};
	if (terminal >= proto::TERMINAL_COUNT) return answer(reply, Status::BAD_REQUEST, NO_LENGTH);

	switch (static_cast<Op>(proto::codeOf(header))) {
	case Op::WRITE:
		if (length == NO_LENGTH || length > proto::CHUNK_BYTES) return answer(reply, Status::BAD_REQUEST, NO_LENGTH);
		proto::unpackBytes(request, bytes, length);
		handleWrite(sender, bytes, length, proto::isLast(header), terminal);
		return answer(reply, Status::OK, length);
	case Op::CLEAR:
		screenClear(terminal);
		return answer(reply, Status::OK, NO_LENGTH);
	case Op::ECHO:
		if (length == NO_LENGTH || length > proto::CHUNK_BYTES) return answer(reply, Status::BAD_REQUEST, NO_LENGTH);
		if (!isTty(sender)) return answer(reply, Status::DENIED, NO_LENGTH);
		proto::unpackBytes(request, bytes, length);
		for (size_t index = 0; index < length; index++)
			screenCharacter(bytes[index], terminal);
		return answer(reply, Status::OK, length);
	case Op::SHOW:
		if (!isTty(sender)) return answer(reply, Status::DENIED, NO_LENGTH);
		screenShow(terminal);
		return answer(reply, Status::OK, NO_LENGTH);
	case Op::READ:
	case Op::ATTACH:
		break;
	}
	answer(reply, Status::BAD_REQUEST, NO_LENGTH);
}

} // namespace cock::console
