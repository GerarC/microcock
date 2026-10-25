#include <cock/abi/capability.hpp>
#include <cock/abi/console.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <console/display.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;
using cock::abi::NO_CAPABILITY_NODE;

namespace proto = cock::abi::console;

namespace cock::console {

namespace {

constexpr uint32_t DISPLAY_RETRIES = 50;
constexpr uint32_t DISPLAY_RETRY_MS = 1;
constexpr size_t DISPLAY_CHUNK_BYTES = 20;

static_assert(proto::CHUNK_BYTES <= DISPLAY_CHUNK_BYTES);

uint32_t display = NO_CAPABILITY_NODE;

void deliverToDisplay(const Message &message) {
	if (display == NO_CAPABILITY_NODE) return;
	for (uint32_t attempt = 0; attempt < DISPLAY_RETRIES; attempt++) {
		if (cock::user::ep_send(display, message)) return;
		cock::user::sleep_ms(DISPLAY_RETRY_MS);
	}
}

} // namespace

bool connectDisplay() {
	display = cock::user::names::connect(proto::DISPLAY_NAME).handle;
	return display != NO_CAPABILITY_NODE;
}

void screenWrite(const char *text, size_t length) {
	for (size_t offset = 0; offset < length; offset += DISPLAY_CHUNK_BYTES) {
		size_t piece = length - offset;
		if (piece > DISPLAY_CHUNK_BYTES) piece = DISPLAY_CHUNK_BYTES;

		Message message = {};
		message.type = MessageType::OUTPUT_WRITE_STR;
		message.payload.args.arg0 = static_cast<uint32_t>(piece);
		char *target = reinterpret_cast<char *>(&message.payload.args.arg1);
		for (size_t index = 0; index < piece; index++)
			target[index] = text[offset + index];
		deliverToDisplay(message);
	}
}

void screenClear() {
	Message message = {};
	message.type = MessageType::NONE;
	deliverToDisplay(message);
}

void screenCharacter(char character) {
	Message message = {};
	message.type = MessageType::OUTPUT_WRITE_CHAR;
	message.payload.args.arg0 = static_cast<uint32_t>(character);
	deliverToDisplay(message);
}

} // namespace cock::console
