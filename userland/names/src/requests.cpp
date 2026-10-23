#include <names/connect.hpp>
#include <names/registration.hpp>
#include <names/requests.hpp>

using cock::abi::names::Op;
using cock::abi::names::Status;

namespace cock::names {

void serve(uint32_t caller, const uint32_t (&words)[abi::names::CALL_WORDS],
		   uint32_t (&reply)[abi::names::CALL_WORDS]) {
	Status status = Status::BAD_REQUEST;
	if (abi::names::opOf(words[abi::names::HEADER_WORD]) == static_cast<uint32_t>(Op::CONNECT)) {
		char name[abi::names::MAX_NAME_BYTES + 1];
		if (abi::names::unpackName(words, name) != 0) status = connect(caller, name, reply);
	} else {
		status = registerService(caller, words);
	}
	reply[abi::names::HEADER_WORD] = static_cast<uint32_t>(status);
}

} // namespace cock::names
