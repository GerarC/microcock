#include <cock/abi/capability.hpp>
#include <cock/user/api.hpp>
#include <names/connect.hpp>
#include <names/grants.hpp>
#include <names/policy.hpp>
#include <names/services.hpp>

using cock::abi::CapabilityType;
using cock::abi::names::Status;

namespace {

constexpr uintptr_t SINGLE_LABEL = 1;

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

	if (!granted(client, server->label)) {
		if (!hasRoom() ||
			!cock::user::cap_grant(client, CapabilityType::IPC, server->label, SINGLE_LABEL))
			return Status::NO_ROOM;
		record(client, server->label);
	}

	reply[abi::names::PID_WORD] = server->pid;
	reply[abi::names::LABEL_WORD] = server->label;
	return Status::OK;
}

} // namespace cock::names
