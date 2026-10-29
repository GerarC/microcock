#include <pipe/pipes.hpp>

namespace cock::pipe {

namespace {

constexpr size_t NO_BYTES = 0;
constexpr size_t START_INDEX = 0;
constexpr uint32_t NO_ENDS = 0;

} // namespace

Pipe pipes[MAX_PIPES];

Pipe *createPipe() {
	for (Pipe &pipe : pipes) {
		if (pipe.used) continue;
		pipe.used = true;
		pipe.head = START_INDEX;
		pipe.count = NO_BYTES;
		pipe.readers = NO_ENDS;
		pipe.writers = NO_ENDS;
		return &pipe;
	}
	return nullptr;
}

size_t pushBytes(Pipe &pipe, const uint8_t *bytes, size_t length) {
	size_t room = PIPE_BYTES - pipe.count;
	size_t taken = length < room ? length : room;
	for (size_t index = 0; index < taken; index++)
		pipe.data[(pipe.head + pipe.count + index) % PIPE_BYTES] = bytes[index];
	pipe.count += taken;
	return taken;
}

size_t popBytes(Pipe &pipe, uint8_t *bytes, size_t length) {
	size_t taken = length < pipe.count ? length : pipe.count;
	for (size_t index = 0; index < taken; index++)
		bytes[index] = pipe.data[(pipe.head + index) % PIPE_BYTES];
	pipe.head = (pipe.head + taken) % PIPE_BYTES;
	pipe.count -= taken;
	return taken;
}

void releaseIfUnused(Pipe &pipe) {
	if (pipe.readers != NO_ENDS || pipe.writers != NO_ENDS) return;
	pipe.used = false;
	pipe.count = NO_BYTES;
}

} // namespace cock::pipe
