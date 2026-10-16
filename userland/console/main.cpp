#include <cock/abi/console.hpp>
#include <cock/abi/console_line.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::abi::console::Echo;
using cock::abi::console::LineDiscipline;
using cock::abi::console::Op;
using cock::abi::console::Status;
using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::user::CallResult;
using cock::user::CallStatus;
using cock::user::NO_PROCESS;

namespace {

namespace console = cock::abi::console;

constexpr uint16_t SERIAL_BASE_PORT = 0x3F8;
constexpr uint32_t SERIAL_PORT_COUNT = 8;
constexpr uint16_t SERIAL_LINE_STATUS = SERIAL_BASE_PORT + 5;
constexpr uint8_t SERIAL_TRANSMIT_EMPTY = 0x20;
constexpr uint32_t SERIAL_SPIN_LIMIT = 100000;
constexpr uint32_t DISPLAY_RETRIES = 50;
constexpr uint32_t DISPLAY_RETRY_MS = 1;
constexpr size_t DISPLAY_CHUNK_BYTES = 20;
constexpr uintptr_t FAILED_STATUS = 1;
constexpr uint32_t ANSWER_WORDS = console::FIRST_DATA_WORD + console::DATA_WORDS;

static_assert(console::CHUNK_BYTES <= DISPLAY_CHUNK_BYTES);

struct Reader {
	uint32_t ticket;
	size_t length;
};

enum class Served {
	REPLY,
	DEFERRED,
};

constexpr size_t MAX_READERS = 8;
constexpr size_t FIRST_READER = 0;
constexpr size_t SLOT_COUNT = 8;
constexpr size_t SLOT_BYTES = 256;

struct Slot {
	bool used;
	uint32_t sender;
	size_t length;
	char bytes[SLOT_BYTES];
};

Slot slots[SLOT_COUNT];
LineDiscipline discipline;
Reader readers[MAX_READERS];
size_t readerCount = 0;
uint32_t display = NO_PROCESS;
uint32_t keyboard = NO_PROCESS;

void serialWrite(const char *text, size_t length) {
	for (size_t index = 0; index < length; index++) {
		for (uint32_t spin = 0; spin < SERIAL_SPIN_LIMIT; spin++) {
			if (cock::user::port_in8(SERIAL_LINE_STATUS) & SERIAL_TRANSMIT_EMPTY) break;
		}
		cock::user::port_out8(SERIAL_BASE_PORT, static_cast<uint8_t>(text[index]));
	}
}

void deliverToDisplay(const Message &message) {
	if (display == NO_PROCESS) return;
	for (uint32_t attempt = 0; attempt < DISPLAY_RETRIES; attempt++) {
		if (cock::user::ipc_send(display, message)) return;
		cock::user::sleep_ms(DISPLAY_RETRY_MS);
	}
}

void screenWrite(const char *text, size_t length) {
	for (size_t offset = 0; offset < length; offset += DISPLAY_CHUNK_BYTES) {
		size_t piece = length - offset;
		if (piece > DISPLAY_CHUNK_BYTES) piece = DISPLAY_CHUNK_BYTES;

		Message message = {};
		message.type = MessageType::VIDEO_WRITE_STR;
		message.payload.args.arg0 = static_cast<uint32_t>(piece);
		char *target = reinterpret_cast<char *>(&message.payload.args.arg1);
		for (size_t index = 0; index < piece; index++)
			target[index] = text[offset + index];
		deliverToDisplay(message);
	}
}

void emit(const char *text, size_t length) {
	serialWrite(text, length);
	screenWrite(text, length);
}

void flush(Slot &slot) {
	emit(slot.bytes, slot.length);
	slot.length = 0;
	slot.used = false;
}

Slot *findSlot(uint32_t sender) {
	Slot *free = nullptr;
	for (Slot &slot : slots) {
		if (slot.used && slot.sender == sender) return &slot;
		if (!slot.used && free == nullptr) free = &slot;
	}
	if (free != nullptr) return free;

	for (Slot &slot : slots) {
		if (cock::user::process_alive(slot.sender)) continue;
		flush(slot);
		return &slot;
	}
	return nullptr;
}

void handleWrite(uint32_t sender, const char *bytes, size_t length, bool last) {
	Slot *slot = findSlot(sender);
	if (slot == nullptr) return emit(bytes, length);

	if (slot->length + length > SLOT_BYTES) {
		emit(slot->bytes, slot->length);
		slot->length = 0;
	}
	for (size_t index = 0; index < length; index++)
		slot->bytes[slot->length + index] = bytes[index];
	slot->length += length;
	slot->used = true;
	slot->sender = sender;
	if (last) flush(*slot);
}

void screenCharacter(char character) {
	Message message = {};
	message.type = MessageType::VIDEO_WRITE_CHAR;
	message.payload.args.arg0 = static_cast<uint32_t>(character);
	deliverToDisplay(message);
}

void echo(Echo kind, char character) {
	switch (kind) {
	case Echo::NONE:
		return;
	case Echo::CHARACTER:
		return screenCharacter(character);
	case Echo::ERASE:
		return screenCharacter(console::CHAR_BACKSPACE);
	case Echo::NEWLINE:
		return screenCharacter(console::CHAR_NEWLINE);
	}
}

void handleMessage(const Message &message) {
	if (message.type != MessageType::CONSOLE_KEY) return;
	if (message.sender_pid == cock::ipc::KERNEL_SENDER) return;

	if (message.sender_pid != keyboard) keyboard = cock::user::lookup(console::KEYBOARD_NAME);
	if (message.sender_pid != keyboard) return;

	char character = static_cast<char>(message.payload.args.arg0);
	echo(discipline.feed(character), character);
}

void drainMessages() {
	Message message = {};
	while (cock::user::ipc_try_recv(message) == cock::user::Received::MESSAGE)
		handleMessage(message);
}

void answer(uint32_t (&words)[ANSWER_WORDS], Status status, size_t count) {
	words[console::HEADER_WORD] = console::pack(static_cast<uint32_t>(status), count);
}

void fillRead(uint32_t (&reply)[ANSWER_WORDS], size_t wanted) {
	char bytes[console::CHUNK_BYTES] = {};
	size_t length = discipline.take(bytes, wanted);
	console::packBytes(reply, bytes, length);
	answer(reply, Status::OK, length);
}

void wakeReaders() {
	while (readerCount > 0 && discipline.hasInput()) {
		Reader reader = readers[FIRST_READER];
		readerCount--;
		for (size_t index = 0; index < readerCount; index++)
			readers[index] = readers[index + 1];

		uint32_t reply[ANSWER_WORDS] = {};
		fillRead(reply, reader.length);
		cock::user::ipc_reply_to(reader.ticket, reply);
	}
}

Served serve(uint32_t sender, const uint32_t (&request)[ANSWER_WORDS],
			 uint32_t (&reply)[ANSWER_WORDS]) {
	for (uint32_t &word : reply)
		word = 0;

	uint32_t header = request[console::HEADER_WORD];
	size_t length = console::lengthOf(header);
	char bytes[console::CHUNK_BYTES] = {};

	switch (static_cast<Op>(console::codeOf(header))) {
	case Op::WRITE:
		if (length == 0 || length > console::CHUNK_BYTES) {
			answer(reply, Status::BAD_REQUEST, 0);
			return Served::REPLY;
		}
		console::unpackBytes(request, bytes, length);
		handleWrite(sender, bytes, length, console::isLast(header));
		answer(reply, Status::OK, length);
		return Served::REPLY;
	case Op::READ:
		if (length == 0 || length > console::CHUNK_BYTES) {
			answer(reply, Status::BAD_REQUEST, 0);
			return Served::REPLY;
		}
		if (discipline.hasInput()) {
			fillRead(reply, length);
			return Served::REPLY;
		}
		if (readerCount < MAX_READERS) {
			uint32_t ticket = cock::user::ipc_defer();
			if (ticket != cock::user::NO_TICKET) {
				readers[readerCount++] = {ticket, length};
				return Served::DEFERRED;
			}
		}
		answer(reply, Status::BUSY, 0);
		return Served::REPLY;
	}
	answer(reply, Status::BAD_REQUEST, 0);
	return Served::REPLY;
}

void waitForCapabilities() {
	Message message = {};
	for (;;) {
		cock::user::ipc_recv(message);
		if (message.type == MessageType::CAPABILITY_GRANTED) {
			display = message.payload.args.arg0;
			return;
		}
	}
}

} // namespace

extern "C" void _start() {
	waitForCapabilities();

	if (!cock::user::request_ports(SERIAL_BASE_PORT, SERIAL_PORT_COUNT)) {
		puts("console: cannot open the serial port");
		cock::user::exit_process(FAILED_STATUS);
	}
	puts("console: ready");

	CallResult result = cock::user::ipc_accept();
	for (;;) {
		if (result.status == CallStatus::MESSAGE_PENDING) {
			drainMessages();
			wakeReaders();
			result = cock::user::ipc_accept();
			continue;
		}
		if (!result.ok) {
			result = cock::user::ipc_accept();
			continue;
		}

		uint32_t reply[ANSWER_WORDS] = {};
		if (serve(result.peer, result.words, reply) == Served::DEFERRED) {
			result = cock::user::ipc_accept();
			continue;
		}
		result = cock::user::ipc_reply_wait(reply);
	}
}
