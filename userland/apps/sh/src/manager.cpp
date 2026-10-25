#include <cock/user/names.hpp>
#include <sh/manager.hpp>

namespace cock::sh {

namespace {

constexpr char SERVER_NAME[] = "pm";

cock::user::pm::Client client;
bool connected = false;

} // namespace

cock::user::pm::Client *processManager() {
	if (!connected) {
		cock::user::names::Connection connection = cock::user::names::connect(SERVER_NAME);
		if (connection.status != cock::abi::names::Status::OK) return nullptr;
		client.connect(connection.handle);
		connected = true;
	}
	return &client;
}

void forgetProcessManager() {
	connected = false;
}

} // namespace cock::sh
