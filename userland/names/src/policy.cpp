#include <names/policy.hpp>

namespace cock::names {

bool mayConnect(const Service &client, const Service &server) {
	return lists(client, server.label) || lists(server, client.label);
}

} // namespace cock::names
