#include <vga_driver/vga.hpp>
#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>

using namespace cock::vga;
using namespace cock::ipc;

namespace {

constexpr size_t STRING_ARGUMENT_WORDS = 5;
constexpr size_t MAX_STRING_CHUNK = STRING_ARGUMENT_WORDS * sizeof(uint32_t);

}

extern "C" int main() {
	bool ready = false;

	Message msg = {};
	while (true) {
		cock::user::ipc_recv(msg);
		switch (msg.type) {
			case MessageType::CAPABILITY_GRANTED:
				if (!ready) ready = VgaServer::init();
				break;
			case MessageType::OUTPUT_WRITE_CHAR:
				if (ready) VgaServer::putChar(static_cast<char>(msg.payload.args.arg0));
				break;
			case MessageType::OUTPUT_WRITE_STR: {
				if (!ready) break;
				size_t length = msg.payload.args.arg0;
				if (length > MAX_STRING_CHUNK) length = MAX_STRING_CHUNK;
				const char *str_chunk =
					reinterpret_cast<const char *>(&msg.payload.args.arg1);
				VgaServer::write(str_chunk, length);
				break;
			}
			case MessageType::OUTPUT_SELECT_TERMINAL:
				VgaServer::selectTarget(msg.payload.args.arg0);
				break;
			case MessageType::OUTPUT_SHOW_TERMINAL:
				if (ready) VgaServer::show(msg.payload.args.arg0);
				break;
			case MessageType::NONE:
				if (ready) VgaServer::clear();
				break;
			default:
				break;
		}
	}
}
