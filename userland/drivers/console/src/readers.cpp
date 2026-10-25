#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <console/input.hpp>
#include <console/readers.hpp>

using cock::abi::console::Status;

namespace proto = cock::abi::console;

namespace cock::console {

namespace {

constexpr size_t MAX_READERS = 8;
constexpr size_t FIRST_READER = 0;
constexpr size_t NO_READERS = 0;

struct Reader {
	uint32_t ticket;
	size_t length;
};

Reader readers[MAX_READERS];
size_t readerCount = NO_READERS;

} // namespace

void fillRead(uint32_t (&reply)[ANSWER_WORDS], size_t wanted) {
	char bytes[proto::CHUNK_BYTES] = {};
	size_t length = discipline.take(bytes, wanted);
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
	while (readerCount > NO_READERS && discipline.hasInput()) {
		Reader reader = readers[FIRST_READER];
		readerCount--;
		for (size_t index = 0; index < readerCount; index++)
			readers[index] = readers[index + 1];

		uint32_t reply[ANSWER_WORDS] = {};
		fillRead(reply, reader.length);
		cock::user::ipc_reply_to(reader.ticket, reply);
	}
}

} // namespace cock::console
