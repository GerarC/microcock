#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/user/api.hpp>
#include <init/endpoints.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::abi::endpoint::MAX_ENDPOINTS;
using cock::abi::endpoint::NO_ENDPOINT;

namespace {

struct Pair {
	uint32_t label = 0;
	uint32_t endpoint = NO_ENDPOINT;
};

constexpr uint32_t NO_LABEL = 0;

Pair pairs[MAX_ENDPOINTS];

} // namespace

namespace cock::init {

uint32_t endpointOf(uint32_t label) {
	Pair *free = nullptr;
	for (Pair &pair : pairs) {
		if (pair.label == label) return pair.endpoint;
		if (free == nullptr && pair.label == NO_LABEL) free = &pair;
	}
	if (free == nullptr) return NO_ENDPOINT;

	uint32_t handle = 0;
	uint32_t endpoint = cock::user::ep_create(handle);
	if (endpoint == NO_ENDPOINT) {
		printf("init: no endpoint for label %u\n", static_cast<unsigned>(label));
		return NO_ENDPOINT;
	}
	free->label = label;
	free->endpoint = endpoint;
	return endpoint;
}

uint32_t bindEndpoint(uint32_t label, uint32_t pid) {
	uint32_t endpoint = endpointOf(label);
	if (endpoint == NO_ENDPOINT || !cock::user::ep_bind(endpoint, pid)) return NO_ENDPOINT;
	return endpoint;
}

uint32_t handleOf(uint32_t label) {
	uint32_t endpoint = endpointOf(label);
	if (endpoint == NO_ENDPOINT) return cock::abi::NO_CAPABILITY_NODE;
	uint32_t self = cock::user::self();
	return cock::user::cap_grant_node(self, cock::abi::CapabilityType::ENDPOINT, endpoint,
									  cock::abi::endpoint::SINGLE_ENDPOINT, false, false, self);
}

bool sendTo(uint32_t label, const cock::ipc::Message &message) {
	uint32_t handle = handleOf(label);
	if (handle == cock::abi::NO_CAPABILITY_NODE) return false;

	bool sent = cock::user::ep_send(handle, message);
	cock::user::cap_revoke(handle);
	return sent;
}

} // namespace cock::init
