#include <kill/outcome.hpp>

namespace cock::kill {

const char *describe(cock::abi::pm::Status status) {
	switch (status) {
	case cock::abi::pm::Status::DENIED:
		return "not permitted";
	case cock::abi::pm::Status::NOT_FOUND:
		return "no such process";
	case cock::abi::pm::Status::BAD_REQUEST:
		return "bad request";
	case cock::abi::pm::Status::FAILED:
		return "refused by the kernel";
	default:
		return "failed";
	}
}

} // namespace cock::kill
