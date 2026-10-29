#include <head/window.hpp>

namespace cock::head {

namespace {

constexpr char NEWLINE = '\n';

}

size_t Window::take(const char *chunk, size_t count) {
	for (size_t index = 0; index < count && remaining != 0; index++)
		if (chunk[index] == NEWLINE && --remaining == 0) return index + 1;
	return remaining == 0 ? 0 : count;
}

} // namespace cock::head
