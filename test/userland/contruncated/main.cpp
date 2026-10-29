#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <cock/user/names.hpp>
#include <stddef.h>
#include <stdint.h>

namespace {

constexpr char FIRST_PART[] = "contruncated";
constexpr char SECOND_PART[] = ": cut\n";
constexpr uintptr_t EXIT_OK = 0;
constexpr uintptr_t EXIT_FAILED = 1;

bool sendWithoutLast(uint32_t handle, const char *text, size_t length) {
	uint32_t words[cock::user::CALL_WORDS] = {};
	words[cock::abi::console::HEADER_WORD] = cock::abi::console::pack(
		static_cast<uint32_t>(cock::abi::console::Op::WRITE), length);
	cock::abi::console::packBytes(words, text, length);
	return cock::user::ep_call(handle, words).ok;
}

}

extern "C" int main() {
	cock::user::names::Connection server = cock::user::names::connect(cock::abi::console::SERVER_NAME);
	if (server.pid == cock::user::NO_PROCESS || server.handle == cock::abi::NO_CAPABILITY_NODE)
		cock::user::exit_process(EXIT_FAILED);

	bool sent = sendWithoutLast(server.handle, FIRST_PART, sizeof(FIRST_PART) - 1) &&
				sendWithoutLast(server.handle, SECOND_PART, sizeof(SECOND_PART) - 1);
	cock::user::exit_process(sent ? EXIT_OK : EXIT_FAILED);
}
