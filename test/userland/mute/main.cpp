#include <cock/ipc/message.hpp>
#include <cock/user/api.hpp>

namespace {

constexpr uintptr_t EXIT_SILENT = 0;

}

extern "C" int main() {
	cock::ipc::Message message = {};
	cock::user::ipc_recv(message);
	cock::user::exit_process(EXIT_SILENT);
}
