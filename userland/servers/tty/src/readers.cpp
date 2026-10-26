#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <tty/input.hpp>
#include <tty/readers.hpp>

using cock::abi::console::Status;

namespace proto = cock::abi::console;

namespace cock::tty {

namespace {

constexpr size_t MAX_READERS = 8;
constexpr size_t FIRST_READER = 0;
constexpr size_t NO_READERS = 0;
constexpr size_t NO_BYTES = 0;

struct Reader {
	uint32_t ticket;
	size_t length;
};

Reader readers[MAX_READERS];
size_t readerCount = NO_READERS;

char returned[proto::CHUNK_BYTES];
size_t returnedLength = NO_BYTES;

size_t takeInput(char *out, size_t wanted) {
	size_t count = NO_BYTES;
	while (count < wanted && count < returnedLength) {
		out[count] = returned[count];
		count++;
	}
	for (size_t index = count; index < returnedLength; index++)
		returned[index - count] = returned[index];
	returnedLength -= count;

	if (count < wanted) count += discipline.take(out + count, wanted - count);
	return count;
}

} // namespace

bool inputReady() {
	return returnedLength > NO_BYTES || discipline.hasInput();
}

void fillRead(uint32_t (&reply)[ANSWER_WORDS], size_t wanted) {
	char bytes[proto::CHUNK_BYTES] = {};
	size_t length = takeInput(bytes, wanted);
	proto::packBytes(reply, bytes, length);
	answer(reply, Status::OK, length);
}

bool deferRead(size_t length) {
	if (readerCount >= MAX_READERS) return false;

	uint32_t ticket = cock::user::ipc_defer();
	if (ticket == cock::user::NO_TICKET) return false;

	readers[readerCount++] = {ticket, length};
	return true;
}

void wakeReaders() {
	while (readerCount > NO_READERS && inputReady()) {
		Reader reader = readers[FIRST_READER];
		readerCount--;
		for (size_t index = 0; index < readerCount; index++)
			readers[index] = readers[index + 1];

		char bytes[proto::CHUNK_BYTES] = {};
		size_t length = takeInput(bytes, reader.length);
		uint32_t reply[ANSWER_WORDS] = {};
		proto::packBytes(reply, bytes, length);
		answer(reply, Status::OK, length);
		if (cock::user::ipc_reply_to(reader.ticket, reply)) continue;

		for (size_t index = 0; index < length; index++)
			returned[index] = bytes[index];
		returnedLength = length;
	}
}

} // namespace cock::tty
