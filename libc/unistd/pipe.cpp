#include "descriptor.hpp"
#include <cock/abi/pipe.hpp>
#include <cock/user/pipe.hpp>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

using cock::libc::CLOSED;
using cock::libc::Description;
using cock::libc::dropDescriptions;
using cock::libc::failWith;
using cock::libc::FAILURE;
using cock::libc::openDescription;
using cock::libc::Operations;

namespace {

constexpr int SUCCESS = 0;
constexpr int READ_END_INDEX = 0;
constexpr int WRITE_END_INDEX = 1;

cock::user::pipe::Client client;

int errorFor(cock::abi::pipe::Status status) {
	using cock::abi::pipe::Status;
	switch (status) {
	case Status::OK:
		return SUCCESS;
	case Status::BAD_REQUEST:
		return EINVAL;
	case Status::BAD_HANDLE:
		return EBADF;
	case Status::NO_ROOM:
		return ENFILE;
	case Status::BROKEN:
		return EPIPE;
	case Status::BUSY:
		return EBUSY;
	case Status::NOT_CONNECTED:
		return ENOTCONN;
	}
	return EIO;
}

const Operations &pipeOperations();

int failFromServer() {
	cock::abi::pipe::Status status = client.lastStatus();
	if (status == cock::abi::pipe::Status::NOT_CONNECTED) dropDescriptions(pipeOperations());
	return failWith(errorFor(status));
}

ssize_t pipeRead(Description &end, void *buf, size_t count) {
	long got = client.read(end.handle, static_cast<uint8_t *>(buf), count);
	return got < 0 ? failFromServer() : static_cast<ssize_t>(got);
}

ssize_t pipeWrite(Description &end, const void *buf, size_t count) {
	long put = client.write(end.handle, static_cast<const uint8_t *>(buf), count);
	return put < 0 ? failFromServer() : static_cast<ssize_t>(put);
}

off_t pipeSeek(Description &, off_t, int) {
	return failWith(ESPIPE);
}

int pipeClose(Description &end) {
	return client.close(end.handle) ? CLOSED : errorFor(client.lastStatus());
}

constexpr Operations PIPE_OPERATIONS = {pipeRead, pipeWrite, pipeSeek, pipeClose};

const Operations &pipeOperations() {
	return PIPE_OPERATIONS;
}

}

extern "C" int pipe(int fds[2]) {
	if (fds == nullptr) return failWith(EFAULT);
	if (!client.connect()) return failWith(ENOTCONN);

	uint32_t readEnd = cock::abi::pipe::NO_END;
	uint32_t writeEnd = cock::abi::pipe::NO_END;
	if (!client.create(readEnd, writeEnd)) return failFromServer();

	int readDescriptor = openDescription(pipeOperations(), readEnd, true, false, false);
	if (readDescriptor == FAILURE) {
		client.close(readEnd);
		client.close(writeEnd);
		return FAILURE;
	}

	int writeDescriptor = openDescription(pipeOperations(), writeEnd, false, true, false);
	if (writeDescriptor == FAILURE) {
		close(readDescriptor);
		client.close(writeEnd);
		return FAILURE;
	}

	fds[READ_END_INDEX] = readDescriptor;
	fds[WRITE_END_INDEX] = writeDescriptor;
	return SUCCESS;
}
