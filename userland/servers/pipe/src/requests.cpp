#include <cock/abi/pipe.hpp>
#include <pipe/ends.hpp>
#include <pipe/requests.hpp>
#include <pipe/waiters.hpp>

using cock::abi::pipe::Op;
using cock::abi::pipe::Status;

namespace proto = cock::abi::pipe;

namespace cock::pipe {

namespace {

constexpr size_t NO_BYTES = 0;
constexpr uint32_t CLEARED_WORD = 0;

void clear(uint32_t (&words)[CALL_WORDS]) {
	for (uint32_t &word : words)
		word = CLEARED_WORD;
}

Attempt read(End &end, size_t length, uint32_t (&reply)[CALL_WORDS]) {
	Pipe &pipe = *end.pipe;
	if (pipe.count == NO_BYTES) {
		if (pipe.writers != 0) return Attempt::BLOCKED;
		answer(reply, Status::OK, NO_BYTES);
		return Attempt::DONE;
	}

	uint8_t bytes[proto::CHUNK_BYTES] = {};
	size_t taken = popBytes(pipe, bytes, length);
	proto::packBytes(reply, bytes, taken);
	answer(reply, Status::OK, taken);
	return Attempt::DONE;
}

Attempt write(End &end, size_t length, const uint32_t (&request)[CALL_WORDS],
			  uint32_t (&reply)[CALL_WORDS]) {
	Pipe &pipe = *end.pipe;
	if (pipe.readers == 0) {
		answer(reply, Status::BROKEN, NO_BYTES);
		return Attempt::DONE;
	}
	if (pipe.count == PIPE_BYTES) return Attempt::BLOCKED;

	uint8_t bytes[proto::CHUNK_BYTES] = {};
	proto::unpackBytes(request, bytes, length);
	answer(reply, Status::OK, pushBytes(pipe, bytes, length));
	return Attempt::DONE;
}

void create(uint32_t sender, uint32_t (&reply)[CALL_WORDS]) {
	uint32_t readHandle = proto::NO_END;
	uint32_t writeHandle = proto::NO_END;
	if (!createEnds(sender, readHandle, writeHandle)) return answer(reply, Status::NO_ROOM, NO_BYTES);

	answer(reply, Status::OK, NO_BYTES);
	reply[proto::FIRST_DATA_WORD] = readHandle;
	reply[proto::SECOND_WORD] = writeHandle;
}

void close(uint32_t sender, uint32_t handle, uint32_t (&reply)[CALL_WORDS]) {
	End *end = findEnd(sender, handle);
	if (end == nullptr) return answer(reply, Status::BAD_HANDLE, NO_BYTES);

	closeEnd(*end);
	answer(reply, Status::OK, NO_BYTES);
}

} // namespace

Attempt attempt(uint32_t sender, const uint32_t (&request)[CALL_WORDS], uint32_t (&reply)[CALL_WORDS]) {
	clear(reply);
	uint32_t header = request[proto::HEADER_WORD];
	Op op = static_cast<Op>(proto::codeOf(header));
	size_t length = proto::lengthOf(header);

	bool isWrite = op == Op::WRITE;
	if ((op != Op::READ && !isWrite) || length == NO_BYTES || length > proto::CHUNK_BYTES) {
		answer(reply, Status::BAD_REQUEST, NO_BYTES);
		return Attempt::DONE;
	}

	End *end = findEnd(sender, proto::endOf(header));
	if (end == nullptr || end->writer != isWrite) {
		answer(reply, Status::BAD_HANDLE, NO_BYTES);
		return Attempt::DONE;
	}
	return isWrite ? write(*end, length, request, reply) : read(*end, length, reply);
}

Served serve(uint32_t sender, const uint32_t (&request)[CALL_WORDS], uint32_t (&reply)[CALL_WORDS]) {
	clear(reply);
	uint32_t header = request[proto::HEADER_WORD];

	switch (static_cast<Op>(proto::codeOf(header))) {
	case Op::CREATE:
		create(sender, reply);
		return Served::REPLY;
	case Op::CLOSE:
		close(sender, proto::endOf(header), reply);
		return Served::REPLY;
	case Op::READ:
	case Op::WRITE:
		if (attempt(sender, request, reply) == Attempt::DONE) return Served::REPLY;
		if (holdCall(sender, request)) return Served::DEFERRED;
		answer(reply, Status::BUSY, NO_BYTES);
		return Served::REPLY;
	}
	answer(reply, Status::BAD_REQUEST, NO_BYTES);
	return Served::REPLY;
}

} // namespace cock::pipe
