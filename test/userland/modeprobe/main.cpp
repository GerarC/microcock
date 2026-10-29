#include <cock/abi/names.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::abi::names::Status;

namespace {

constexpr char OWNED_NAME[] = "owned";
constexpr char LOCKED_NAME[] = "locked";
constexpr char PUBLIC_NAME[] = "console";
constexpr size_t ATTEMPTS = 50;
constexpr uint32_t RETRY_MILLISECONDS = 100;
constexpr uintptr_t EXIT_PASSED = 0;
constexpr uintptr_t EXIT_FAILED = 1;

bool isStarting(Status status) {
	return status == Status::NOT_FOUND || status == Status::UNKNOWN_CLIENT;
}

Status connectWhenRunning(const char *name) {
	Status status = cock::user::names::connect(name).status;
	for (size_t attempt = 0; attempt < ATTEMPTS && isStarting(status); attempt++) {
		cock::user::sleep_ms(RETRY_MILLISECONDS);
		status = cock::user::names::connect(name).status;
	}
	return status;
}

} // namespace

extern "C" int main() {
	bool ownerAllowed = connectWhenRunning(OWNED_NAME) == Status::OK;
	bool strangerRefused = connectWhenRunning(LOCKED_NAME) == Status::DENIED;
	bool othersAllowed = connectWhenRunning(PUBLIC_NAME) == Status::OK;
	bool passed = ownerAllowed && strangerRefused && othersAllowed;

	puts(passed ? "modeprobe: ok" : "modeprobe: owner or mode not applied");
	cock::user::exit_process(passed ? EXIT_PASSED : EXIT_FAILED);
}
