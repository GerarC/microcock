#include <names/policy.hpp>

namespace cock::names {

bool permits(const Service &client, const Service &server) {
	bool owner = server.owner != abi::service::NO_SERVICE && client.label == server.owner;
	uint32_t bit = owner ? abi::service::MODE_OWNER : abi::service::MODE_OTHER;
	return (server.mode & bit) != 0;
}

bool mayConnect(const Service &client, const Service &server) {
	return (lists(client, server.label) || lists(server, client.label)) && permits(client, server);
}

} // namespace cock::names
