#include <console/reply.hpp>

using cock::abi::console::Status;

namespace proto = cock::abi::console;

namespace cock::console {

void answer(uint32_t (&words)[ANSWER_WORDS], Status status, size_t count) {
	words[proto::HEADER_WORD] = proto::pack(static_cast<uint32_t>(status), count);
}

} // namespace cock::console
