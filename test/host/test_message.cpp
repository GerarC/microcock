#include <cock/ipc/message.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using cock::ipc::MAX_ARGUMENTS;
using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

constexpr size_t HEADER_FIELDS = 5;
constexpr size_t FIELD_SIZE = sizeof(uint32_t);
constexpr size_t MINIMUM_SIZE =
	HEADER_FIELDS * FIELD_SIZE + MAX_ARGUMENTS * FIELD_SIZE;
constexpr size_t SENDER_INDEX = 0;
constexpr size_t TARGET_INDEX = 1;
constexpr size_t TYPE_INDEX = 2;
constexpr size_t REPLY_INDEX = 3;
constexpr size_t BADGE_INDEX = 4;
constexpr uint32_t FIRST_ARGUMENT = 0x11;
constexpr uint32_t LAST_ARGUMENT = 0x66;
constexpr size_t LAST_INDEX = MAX_ARGUMENTS - 1;
constexpr uint32_t EXPECTED_PING = 1;
constexpr uint32_t EXPECTED_IRQ_NOTIFICATION = 7;
constexpr uint32_t EXPECTED_CAPABILITY_GRANTED = 8;
constexpr uint32_t EXPECTED_KERNEL_SENDER = 0;

static_assert(sizeof(MessageType) == FIELD_SIZE);

void checkLayout() {
	CHECK_TRUE(sizeof(Message) >= MINIMUM_SIZE);
	CHECK_EQ(SENDER_INDEX * FIELD_SIZE, offsetof(Message, sender_pid));
	CHECK_EQ(TARGET_INDEX * FIELD_SIZE, offsetof(Message, target_pid));
	CHECK_EQ(TYPE_INDEX * FIELD_SIZE, offsetof(Message, type));
	CHECK_EQ(REPLY_INDEX * FIELD_SIZE, offsetof(Message, reply));
	CHECK_EQ(BADGE_INDEX * FIELD_SIZE, offsetof(Message, badge));
}

void checkArgumentAliasing() {
	Message message;
	memset(&message, 0, sizeof(message));
	message.payload.args.arg0 = FIRST_ARGUMENT;
	message.payload.args.arg5 = LAST_ARGUMENT;
	CHECK_EQ(FIRST_ARGUMENT, message.payload.argv[0]);
	CHECK_EQ(LAST_ARGUMENT, message.payload.argv[LAST_INDEX]);
}

void checkTypeNumbers() {
	CHECK_EQ(EXPECTED_PING, static_cast<uint32_t>(MessageType::SYS_PING));
	CHECK_EQ(EXPECTED_IRQ_NOTIFICATION,
			 static_cast<uint32_t>(MessageType::IRQ_NOTIFICATION));
	CHECK_EQ(EXPECTED_CAPABILITY_GRANTED,
			 static_cast<uint32_t>(MessageType::CAPABILITY_GRANTED));
	CHECK_EQ(EXPECTED_KERNEL_SENDER, cock::ipc::KERNEL_SENDER);
}

} // namespace

void testMessage() {
	checkLayout();
	checkArgumentAliasing();
	checkTypeNumbers();
}

} // namespace host_test
