#include <cock/abi/capability.hpp>
#include <cock/abi/endpoint.hpp>
#include <cock/user/api.hpp>
#include <names/connect.hpp>
#include <names/grants.hpp>
#include <names/policy.hpp>
#include <names/services.hpp>

using cock::abi::CapabilityType;
using cock::abi::NO_CAPABILITY_NODE;
using cock::abi::names::Status;

namespace {

constexpr bool NO_DELEGATE = false;
constexpr bool NO_MINT = false;

} // namespace

namespace cock::names {

Status connect(uint32_t client, const char *name, uint32_t (&reply)[abi::names::CALL_WORDS]) {
	const Service *caller = findByPid(client);
	if (caller == nullptr) return Status::UNKNOWN_CLIENT;

	const Service *server = findByName(name);
	if (server == nullptr) return Status::NOT_FOUND;
	if (!mayConnect(*caller, *server)) return Status::DENIED;
	if (server->pid == NOT_RUNNING || !cock::user::process_alive(server->pid))
		return Status::NOT_FOUND;

	uint32_t handle = NO_CAPABILITY_NODE;
	if (granted(client, server->label)) {
		handle = handleOf(client, server->label);
	} else {
		if (server->endpoint == abi::endpoint::NO_ENDPOINT) return Status::NOT_FOUND;
		if (!hasRoom()) return Status::NO_ROOM;
		handle = cock::user::cap_grant_node(client, CapabilityType::ENDPOINT, server->endpoint,
											abi::endpoint::SINGLE_ENDPOINT, NO_DELEGATE, NO_MINT,
											client);
		if (handle == NO_CAPABILITY_NODE) return Status::NO_ROOM;
		record(client, server->label, handle);
	}

	reply[abi::names::PID_WORD] = server->pid;
	reply[abi::names::LABEL_WORD] = server->label;
	reply[abi::names::HANDLE_WORD] = handle;
	return Status::OK;
}

} // namespace cock::names
