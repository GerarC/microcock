#include <cock/user/api.hpp>
#include <names/assembly.hpp>
#include <names/connect.hpp>
#include <names/parked.hpp>
#include <names/paths.hpp>
#include <names/registration.hpp>
#include <names/requests.hpp>

using cock::abi::names::Op;
using cock::abi::names::Status;

namespace cock::names {

namespace {

Status serveName(uint32_t caller, uint32_t process, const uint32_t (&words)[abi::names::CALL_WORDS],
				 uint32_t (&reply)[abi::names::CALL_WORDS], bool &parked) {
	uint32_t header = words[abi::names::HEADER_WORD];
	bool define = abi::names::opOf(header) == static_cast<uint32_t>(Op::DEFINE);
	if (define && process != abi::service::ROOT_PID) return Status::NOT_ROOT;

	char name[abi::names::MAX_NAME_BYTES + 1];
	switch (addPart(caller, header, words, cock::user::process_alive, define, name)) {
	case Assembled::MORE:
		return Status::MORE;
	case Assembled::BAD:
		return Status::BAD_REQUEST;
	case Assembled::DONE:
		break;
	}

	if (define) return defineProgram(process, abi::names::labelOf(header), name);

	char path[MAX_PATH_BYTES + 1];
	normalize(name, path);
	Status status = connect(process, path, reply);
	parked = status == Status::UNKNOWN_CLIENT && park(process, path);
	return status;
}

} // namespace

bool serve(uint32_t caller, const uint32_t (&words)[abi::names::CALL_WORDS],
		   uint32_t (&reply)[abi::names::CALL_WORDS]) {
	uint32_t process = caller;
	uint32_t op = abi::names::opOf(words[abi::names::HEADER_WORD]);

	bool parked = false;
	Status status;
	if (op == static_cast<uint32_t>(Op::CONNECT) || op == static_cast<uint32_t>(Op::DEFINE)) {
		status = serveName(caller, process, words, reply, parked);
	} else {
		status = registerService(process, words);
		if (status == Status::OK &&
			(op == static_cast<uint32_t>(Op::BIND) || op == static_cast<uint32_t>(Op::FORKED)))
			answerParked(words[abi::names::PID_WORD]);
	}
	reply[abi::names::HEADER_WORD] = static_cast<uint32_t>(status);
	return !parked;
}

} // namespace cock::names
