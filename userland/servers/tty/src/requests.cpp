#include <cock/abi/console.hpp>
#include <tty/readers.hpp>
#include <tty/requests.hpp>
#include <tty/terminal.hpp>

using cock::abi::console::Op;
using cock::abi::console::Status;

namespace proto = cock::abi::console;

namespace cock::tty {

namespace {

constexpr size_t NO_LENGTH = 0;
constexpr uint32_t CLEARED_WORD = 0;

Served read(uint32_t header, uint32_t (&reply)[ANSWER_WORDS]) {
	size_t length = proto::lengthOf(header);
	uint32_t number = proto::terminalOf(header);
	if (length == NO_LENGTH || length > proto::CHUNK_BYTES || number >= proto::TERMINAL_COUNT) {
		answer(reply, Status::BAD_REQUEST, NO_LENGTH);
		return Served::REPLY;
	}

	Terminal &terminal = terminals[number];
	if (inputReady(terminal)) {
		fillRead(terminal, reply, length);
		return Served::REPLY;
	}
	if (deferRead(terminal, length)) return Served::DEFERRED;
	answer(reply, Status::BUSY, NO_LENGTH);
	return Served::REPLY;
}

} // namespace

Served serve(uint32_t sender, const uint32_t (&request)[ANSWER_WORDS], uint32_t (&reply)[ANSWER_WORDS]) {
	for (uint32_t &word : reply)
		word = CLEARED_WORD;

	uint32_t header = request[proto::HEADER_WORD];
	switch (static_cast<Op>(proto::codeOf(header))) {
	case Op::READ:
		return read(header, reply);
	case Op::ATTACH: {
		uint32_t number = proto::FIRST_TERMINAL;
		if (!attach(sender, number)) answer(reply, Status::BUSY, NO_LENGTH);
		else answer(reply, Status::OK, number);
		return Served::REPLY;
	}
	case Op::WRITE:
	case Op::CLEAR:
	case Op::ECHO:
	case Op::SHOW:
		break;
	}
	answer(reply, Status::BAD_REQUEST, NO_LENGTH);
	return Served::REPLY;
}

} // namespace cock::tty
