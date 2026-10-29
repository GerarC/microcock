#include <cock/user/names.hpp>
#include <ps/manager.hpp>

namespace cock::ps {

namespace {

constexpr char SERVER_NAME[] = "pm";

}

bool connectManager(cock::user::pm::Client &client) {
	cock::user::names::Connection connection = cock::user::names::connect(SERVER_NAME);
	if (connection.status != cock::abi::names::Status::OK) return false;
	client.connect(connection.handle);
	return true;
}

} // namespace cock::ps
