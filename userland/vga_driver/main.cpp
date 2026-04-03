#include "vga.hpp"
#include <cock/ipc/message.hpp>
#include <sys/syscalls.h>

using namespace cock::vga;
using namespace cock::ipc;

static inline uintptr_t ipc_recv(Message *msg) {
	return syscall(SYS_IPC_RECV, (uintptr_t)msg, 0, 0, 0, 0, 0);
}

extern "C" void _start() {
	VgaServer::init();

	Message msg = {};
	while (true) {
		ipc_recv(&msg);
		switch (msg.type) {
			case MessageType::VIDEO_WRITE_CHAR:
				VgaServer::putChar(static_cast<char>(msg.payload.args.arg0));
				break;
			case MessageType::VIDEO_WRITE_STR: {
				size_t length = msg.payload.args.arg0;
				const char *str_chunk =
					reinterpret_cast<const char *>(&msg.payload.args.arg1);
				VgaServer::write(str_chunk, length);
				break;
			}
			case MessageType::NONE:
				VgaServer::clear();
				break;
			default:
				break;
		}
	}

}
