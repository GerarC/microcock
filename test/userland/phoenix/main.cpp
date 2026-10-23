#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>

using cock::ipc::Message;
using cock::ipc::MessageType;

namespace {

constexpr uintptr_t EXIT_SERVED = 0;

}

extern "C" int main() {
	Message request = {};
	while (cock::user::ipc_recv(request)) {
		if (request.type != MessageType::SYS_PING) continue;
		Message reply = {};
		reply.type = MessageType::SYS_PONG;
		cock::user::ipc_send(request.sender_pid, reply);
		cock::user::exit_process(EXIT_SERVED);
	}
	cock::user::exit_process(EXIT_SERVED);
}
