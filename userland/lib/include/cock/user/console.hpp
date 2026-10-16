#ifndef COCK_USER_CONSOLE_HPP
#define COCK_USER_CONSOLE_HPP

#include <cock/abi/console.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::user::console {

constexpr uint32_t READ_POLL_MS = 10;
constexpr long FAILED = -1;

class Client {
  public:
	bool connect() {
		if (server != NO_PROCESS && process_alive(server)) return true;
		server = lookup(abi::console::SERVER_NAME);
		return server != NO_PROCESS;
	}

	long write(const char *text, size_t length) {
		size_t written = 0;
		while (written < length) {
			size_t step = length - written;
			if (step > abi::console::CHUNK_BYTES) step = abi::console::CHUNK_BYTES;

			uint32_t words[CALL_WORDS] = {};
			bool last = written + step == length;
			words[abi::console::HEADER_WORD] =
				abi::console::pack(static_cast<uint32_t>(abi::console::Op::WRITE), step, last);
			abi::console::packBytes(words, text + written, step);

			CallResult result = ipc_call(server, words);
			if (!result.ok || !accepted(result, step)) return written == 0 ? FAILED : static_cast<long>(written);
			written += step;
		}
		return static_cast<long>(written);
	}

	long read(char *buffer, size_t length) {
		if (length == 0) return 0;
		if (length > abi::console::CHUNK_BYTES) length = abi::console::CHUNK_BYTES;

		for (;;) {
			uint32_t words[CALL_WORDS] = {};
			words[abi::console::HEADER_WORD] =
				abi::console::pack(static_cast<uint32_t>(abi::console::Op::READ), length);

			CallResult result = ipc_call(server, words);
			if (!result.ok) return FAILED;
			if (statusOf(result) == abi::console::Status::BUSY) {
				sleep_ms(READ_POLL_MS);
				continue;
			}
			if (statusOf(result) != abi::console::Status::OK) return FAILED;

			size_t count = abi::console::lengthOf(result.words[abi::console::HEADER_WORD]);
			if (count > length) return FAILED;
			if (count > 0) {
				abi::console::unpackBytes(result.words, buffer, count);
				return static_cast<long>(count);
			}
			sleep_ms(READ_POLL_MS);
		}
	}

  private:
	static abi::console::Status statusOf(const CallResult &result) {
		return static_cast<abi::console::Status>(
			abi::console::codeOf(result.words[abi::console::HEADER_WORD]));
	}

	static bool accepted(const CallResult &result, size_t expected) {
		return statusOf(result) == abi::console::Status::OK &&
			   abi::console::lengthOf(result.words[abi::console::HEADER_WORD]) == expected;
	}

	uint32_t server = NO_PROCESS;
};

} // namespace cock::user::console

#endif // !COCK_USER_CONSOLE_HPP
