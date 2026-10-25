#include <pipe/reply.hpp>

namespace proto = cock::abi::pipe;

namespace cock::pipe {

void answer(uint32_t (&words)[CALL_WORDS], proto::Status status, size_t count) {
	words[proto::HEADER_WORD] = proto::pack(static_cast<uint32_t>(status), count);
}

} // namespace cock::pipe
