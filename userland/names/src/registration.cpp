#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <names/registration.hpp>
#include <names/services.hpp>

using cock::abi::names::Op;
using cock::abi::names::Status;

namespace cock::names {

namespace {

Status define(uint32_t label, const uint32_t (&words)[abi::names::CALL_WORDS]) {
	char name[abi::names::MAX_NAME_BYTES + 1];
	abi::names::unpackName(words, name);
	return defineService(label, name) != nullptr ? Status::OK : Status::BAD_REQUEST;
}

Status allow(uint32_t label, uint32_t other) {
	Service *from = findByLabel(label);
	if (from == nullptr) return Status::BAD_REQUEST;
	return allowTalk(*from, other) ? Status::OK : Status::BAD_REQUEST;
}

Status bind(uint32_t label, uint32_t pid) {
	Service *service = findByLabel(label);
	if (service == nullptr || !cock::user::process_alive(pid)) return Status::BAD_REQUEST;

	const Service *running = findByPid(pid);
	if (running != nullptr && running != service) return Status::BAD_REQUEST;
	service->pid = pid;
	return Status::OK;
}

} // namespace

Status registerService(uint32_t caller, const uint32_t (&words)[abi::names::CALL_WORDS]) {
	if (caller != abi::service::ROOT_PID) return Status::NOT_ROOT;

	uint32_t header = words[abi::names::HEADER_WORD];
	uint32_t label = abi::names::labelOf(header);
	switch (abi::names::opOf(header)) {
	case static_cast<uint32_t>(Op::DEFINE):
		return define(label, words);
	case static_cast<uint32_t>(Op::ALLOW):
		return allow(label, words[abi::names::PID_WORD]);
	case static_cast<uint32_t>(Op::BIND):
		return bind(label, words[abi::names::PID_WORD]);
	default:
		return Status::BAD_REQUEST;
	}
}

} // namespace cock::names
