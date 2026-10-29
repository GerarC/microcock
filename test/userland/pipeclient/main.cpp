#include <cock/abi/pipe.hpp>
#include <cock/user/api.hpp>
#include <cock/user/joinable_thread.hpp>
#include <cock/user/pipe.hpp>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using cock::abi::pipe::Status;
using cock::user::JoinableThread;

namespace {

constexpr char HELLO[] = "hello pipe";
constexpr size_t HELLO_BYTES = sizeof(HELLO) - sizeof(char);
constexpr size_t BUFFER_BYTES = 64;
constexpr size_t TRANSFER_BYTES = 1024;
constexpr size_t PATTERN_PERIOD = 251;
constexpr size_t STACK_BYTES = 0x4000;
constexpr size_t STACK_ALIGNMENT = 16;
constexpr uint32_t LATE_WRITE_MS = 50;
constexpr uint32_t DEADLINE_MS = 5000;
constexpr uint32_t UNKNOWN_END = 777;
constexpr int STDOUT_DESCRIPTOR = 1;
constexpr int NO_OFFSET = 0;
constexpr int PAIR_SIZE = 2;
constexpr int READ_SIDE = 0;
constexpr int WRITE_SIDE = 1;
constexpr int NOTHING = 0;
constexpr int EXIT_SERVED = 0;
constexpr int EXIT_REFUSED = 1;
constexpr ssize_t END_OF_DATA = 0;

alignas(STACK_ALIGNMENT) uint8_t stack[STACK_BYTES];
JoinableThread helper;

struct Transfer {
	int descriptor;
	bool ok;
};

bool failsWith(ssize_t result, int expected) { return result < 0 && errno == expected; }

uint8_t patternAt(size_t index) { return static_cast<uint8_t>(index % PATTERN_PERIOD); }

bool writesHello(int descriptor) {
	return write(descriptor, HELLO, HELLO_BYTES) == static_cast<ssize_t>(HELLO_BYTES);
}

bool readsHello(int descriptor) {
	char buffer[BUFFER_BYTES] = {};
	return read(descriptor, buffer, sizeof(buffer)) == static_cast<ssize_t>(HELLO_BYTES) &&
		   memcmp(buffer, HELLO, HELLO_BYTES) == 0;
}

bool atEnd(int descriptor) {
	char buffer[BUFFER_BYTES] = {};
	return read(descriptor, buffer, sizeof(buffer)) == END_OF_DATA;
}

bool roundTrip() {
	int ends[PAIR_SIZE];
	if (pipe(ends) != 0) return false;

	char buffer[BUFFER_BYTES] = {};
	bool ok = ends[READ_SIDE] != ends[WRITE_SIDE] && writesHello(ends[WRITE_SIDE]) &&
			  readsHello(ends[READ_SIDE]);
	ok = ok && failsWith(lseek(ends[READ_SIDE], NO_OFFSET, SEEK_CUR), ESPIPE) &&
		 failsWith(read(ends[WRITE_SIDE], buffer, sizeof(buffer)), EBADF) &&
		 failsWith(write(ends[READ_SIDE], buffer, sizeof(buffer)), EBADF);
	ok = ok && close(ends[WRITE_SIDE]) == 0 && atEnd(ends[READ_SIDE]);
	return close(ends[READ_SIDE]) == 0 && ok;
}

bool reportsBrokenPipe() {
	int ends[PAIR_SIZE];
	if (pipe(ends) != 0) return false;

	bool ok = close(ends[READ_SIDE]) == 0;
	ok = ok && failsWith(write(ends[WRITE_SIDE], HELLO, HELLO_BYTES), EPIPE);
	return close(ends[WRITE_SIDE]) == 0 && ok;
}

bool keepsEndOpenThroughDup() {
	int ends[PAIR_SIZE];
	if (pipe(ends) != 0) return false;

	int copy = dup(ends[WRITE_SIDE]);
	bool ok = copy >= 0 && close(ends[WRITE_SIDE]) == 0 && writesHello(copy) && readsHello(ends[READ_SIDE]);
	ok = ok && close(copy) == 0 && atEnd(ends[READ_SIDE]);
	return close(ends[READ_SIDE]) == 0 && ok;
}

bool redirectsOutputIntoPipe() {
	int ends[PAIR_SIZE];
	if (pipe(ends) != 0) return false;

	int saved = dup(STDOUT_DESCRIPTOR);
	bool ok = saved >= 0 && dup2(ends[WRITE_SIDE], STDOUT_DESCRIPTOR) == STDOUT_DESCRIPTOR;
	ok = ok && writesHello(STDOUT_DESCRIPTOR);
	if (saved >= 0) ok = dup2(saved, STDOUT_DESCRIPTOR) == STDOUT_DESCRIPTOR && ok;
	if (saved >= 0) ok = close(saved) == 0 && ok;
	ok = ok && close(ends[WRITE_SIDE]) == 0 && readsHello(ends[READ_SIDE]) && atEnd(ends[READ_SIDE]);
	return close(ends[READ_SIDE]) == 0 && ok;
}

void lateWriter(void *argument) {
	cock::user::sleep_ms(LATE_WRITE_MS);
	Transfer &transfer = *static_cast<Transfer *>(argument);
	transfer.ok = writesHello(transfer.descriptor);
}

bool wakesBlockedReader() {
	int ends[PAIR_SIZE];
	if (pipe(ends) != 0) return false;

	Transfer transfer = {ends[WRITE_SIDE], false};
	bool ok = helper.start(lateWriter, &transfer, stack, sizeof(stack));
	ok = ok && readsHello(ends[READ_SIDE]);
	ok = helper.join(DEADLINE_MS) && ok && transfer.ok;
	ok = close(ends[WRITE_SIDE]) == 0 && ok;
	return close(ends[READ_SIDE]) == 0 && ok;
}

void drain(void *argument) {
	Transfer &transfer = *static_cast<Transfer *>(argument);
	uint8_t buffer[BUFFER_BYTES];
	size_t total = 0;
	transfer.ok = true;
	while (total < TRANSFER_BYTES) {
		ssize_t got = read(transfer.descriptor, buffer, sizeof(buffer));
		if (got <= END_OF_DATA) {
			transfer.ok = false;
			return;
		}
		for (ssize_t index = 0; index < got; index++)
			if (buffer[index] != patternAt(total + static_cast<size_t>(index))) transfer.ok = false;
		total += static_cast<size_t>(got);
	}
}

bool fillsAndDrains() {
	int ends[PAIR_SIZE];
	if (pipe(ends) != 0) return false;

	uint8_t bytes[TRANSFER_BYTES];
	for (size_t index = 0; index < TRANSFER_BYTES; index++)
		bytes[index] = patternAt(index);

	Transfer transfer = {ends[READ_SIDE], false};
	bool ok = helper.start(drain, &transfer, stack, sizeof(stack));
	ok = ok && write(ends[WRITE_SIDE], bytes, sizeof(bytes)) == static_cast<ssize_t>(sizeof(bytes));
	ok = helper.join(DEADLINE_MS) && ok && transfer.ok;
	ok = close(ends[WRITE_SIDE]) == 0 && ok;
	return close(ends[READ_SIDE]) == 0 && ok;
}

bool refusesForeignEnds() {
	cock::user::pipe::Client client;
	uint32_t readEnd = cock::abi::pipe::NO_END;
	uint32_t writeEnd = cock::abi::pipe::NO_END;
	if (!client.connect() || !client.create(readEnd, writeEnd)) return false;

	uint8_t buffer[BUFFER_BYTES] = {};
	bool ok = client.read(writeEnd, buffer, sizeof(buffer)) == cock::user::pipe::FAILED &&
			  client.lastStatus() == Status::BAD_HANDLE;
	ok = ok && client.write(readEnd, buffer, HELLO_BYTES) == cock::user::pipe::FAILED &&
		 client.lastStatus() == Status::BAD_HANDLE;
	ok = ok && client.read(UNKNOWN_END, buffer, sizeof(buffer)) == cock::user::pipe::FAILED &&
		 client.lastStatus() == Status::BAD_HANDLE;
	ok = ok && !client.close(UNKNOWN_END) && client.lastStatus() == Status::BAD_HANDLE;
	ok = ok && client.close(readEnd) && client.close(writeEnd);
	return ok && !client.close(readEnd) && client.lastStatus() == Status::BAD_HANDLE;
}

bool refusesNullArray() {
	return failsWith(pipe(nullptr), EFAULT);
}

}

extern "C" int main() {
	bool served = roundTrip();
	if (served) puts("pipeclient: round trip ok");
	served = served && reportsBrokenPipe() && keepsEndOpenThroughDup();
	if (served) puts("pipeclient: ends ok");
	served = served && redirectsOutputIntoPipe();
	if (served) puts("pipeclient: redirect ok");
	served = served && wakesBlockedReader() && fillsAndDrains();
	if (served) puts("pipeclient: blocking ok");
	served = served && refusesForeignEnds() && refusesNullArray();
	if (served) puts("pipeclient: refusals ok");

	puts(served ? "pipeclient: ok" : "pipeclient: refused");
	_exit(served ? EXIT_SERVED : EXIT_REFUSED);
	return NOTHING;
}
