#include <cock/user/api.hpp>
#include <pipe/ends.hpp>

namespace cock::pipe {

namespace {

constexpr uint32_t FIRST_HANDLE = 0;
constexpr uint32_t ONE_END = 1;
constexpr size_t ENDS_PER_PIPE = 2;

End ends[MAX_ENDS];

End *freeEnd() {
	for (End &end : ends)
		if (!end.used) return &end;
	return nullptr;
}

uint32_t lowestFreeHandle(uint32_t owner) {
	uint32_t handle = FIRST_HANDLE;
	while (findEnd(owner, handle) != nullptr)
		handle++;
	return handle;
}

void fill(End &end, uint32_t owner, uint32_t handle, Pipe &pipe, bool writer) {
	end = {true, owner, handle, &pipe, writer};
	if (writer) pipe.writers += ONE_END;
	else pipe.readers += ONE_END;
}

bool hasRoom() {
	size_t freeEnds = 0;
	for (const End &end : ends)
		if (!end.used) freeEnds++;
	bool pipeFree = false;
	for (const Pipe &pipe : pipes)
		if (!pipe.used) pipeFree = true;
	return pipeFree && freeEnds >= ENDS_PER_PIPE;
}

} // namespace

End *findEnd(uint32_t owner, uint32_t handle) {
	for (End &end : ends)
		if (end.used && end.owner == owner && end.handle == handle) return &end;
	return nullptr;
}

bool createEnds(uint32_t owner, uint32_t &readHandle, uint32_t &writeHandle) {
	if (!hasRoom()) releaseDeadOwners();
	if (!hasRoom()) return false;

	Pipe *pipe = createPipe();
	End *readEnd = freeEnd();
	readHandle = lowestFreeHandle(owner);
	fill(*readEnd, owner, readHandle, *pipe, false);

	End *writeEnd = freeEnd();
	writeHandle = lowestFreeHandle(owner);
	fill(*writeEnd, owner, writeHandle, *pipe, true);
	return true;
}

void closeEnd(End &end) {
	Pipe &pipe = *end.pipe;
	if (end.writer) pipe.writers -= ONE_END;
	else pipe.readers -= ONE_END;
	end = {};
	releaseIfUnused(pipe);
}

bool releaseDeadOwners() {
	bool closed = false;
	for (End &end : ends) {
		if (!end.used || cock::user::process_alive(end.owner)) continue;
		closeEnd(end);
		closed = true;
	}
	return closed;
}

bool anyEnd() {
	for (const End &end : ends)
		if (end.used) return true;
	return false;
}

} // namespace cock::pipe
