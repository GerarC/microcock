#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>

extern "C" int main() {
	cock::ipc::Message message = {};
	for (;;)
		cock::user::ipc_recv(message);
}
