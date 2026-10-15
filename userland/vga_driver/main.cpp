#include "vga.hpp"
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>

using namespace cock::vga;
using namespace cock::ipc;

extern "C" void _start() {
	bool ready = false;

	Message msg = {};
	while (true) {
		cock::user::ipc_recv(msg);
		switch (msg.type) {
			case MessageType::CAPABILITY_GRANTED:
				if (!ready) ready = VgaServer::init();
				break;
			case MessageType::VIDEO_WRITE_CHAR:
				if (ready) VgaServer::putChar(static_cast<char>(msg.payload.args.arg0));
				break;
			case MessageType::VIDEO_WRITE_STR: {
				if (!ready) break;
				size_t length = msg.payload.args.arg0;
				const char *str_chunk =
					reinterpret_cast<const char *>(&msg.payload.args.arg1);
				VgaServer::write(str_chunk, length);
				break;
			}
			case MessageType::NONE:
				if (ready) VgaServer::clear();
				break;
			default:
				break;
		}
	}
}
