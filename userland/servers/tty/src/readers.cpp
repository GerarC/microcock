#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <tty/readers.hpp>

using cock::abi::console::Status;

namespace proto = cock::abi::console;

namespace cock::tty {

namespace {

constexpr size_t FIRST_READER = 0;
constexpr size_t NO_READERS = 0;
constexpr size_t NO_BYTES = 0;

size_t takeInput(Terminal &terminal, char *out, size_t wanted) {
	size_t count = NO_BYTES;
	while (count < wanted && count < terminal.returnedLength) {
		out[count] = terminal.returned[count];
		count++;
	}
	for (size_t index = count; index < terminal.returnedLength; index++)
		terminal.returned[index - count] = terminal.returned[index];
	terminal.returnedLength -= count;

	if (count < wanted) count += terminal.discipline.take(out + count, wanted - count);
	return count;
}

void wake(Terminal &terminal) {
	while (terminal.readerCount > NO_READERS && inputReady(terminal)) {
		Reader reader = terminal.readers[FIRST_READER];
		terminal.readerCount--;
		for (size_t index = 0; index < terminal.readerCount; index++)
			terminal.readers[index] = terminal.readers[index + 1];

		char bytes[proto::CHUNK_BYTES] = {};
		size_t length = takeInput(terminal, bytes, reader.length);
		uint32_t reply[ANSWER_WORDS] = {};
		proto::packBytes(reply, bytes, length);
		answer(reply, Status::OK, length);
		if (cock::user::ipc_reply_to(reader.ticket, reply)) continue;

		for (size_t index = 0; index < length; index++)
			terminal.returned[index] = bytes[index];
		terminal.returnedLength = length;
	}
}

} // namespace

bool inputReady(const Terminal &terminal) {
	return terminal.returnedLength > NO_BYTES || terminal.discipline.hasInput();
}

void fillRead(Terminal &terminal, uint32_t (&reply)[ANSWER_WORDS], size_t wanted) {
	char bytes[proto::CHUNK_BYTES] = {};
	size_t length = takeInput(terminal, bytes, wanted);
	proto::packBytes(reply, bytes, length);
	answer(reply, Status::OK, length);
}

bool deferRead(Terminal &terminal, size_t length) {
	if (terminal.readerCount >= MAX_READERS) return false;

	uint32_t ticket = cock::user::ipc_defer();
	if (ticket == cock::user::NO_TICKET) return false;

	terminal.readers[terminal.readerCount++] = {ticket, length};
	return true;
}

void wakeReaders() {
	for (Terminal &terminal : terminals) wake(terminal);
}

} // namespace cock::tty
