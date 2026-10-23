#include <cock/user/constructors.hpp>

using Constructor = void (*)();

extern "C" Constructor __init_array_start[];
extern "C" Constructor __init_array_end[];

namespace cock::user {

void runConstructors() {
	for (Constructor *entry = __init_array_start; entry < __init_array_end; entry++) {
		(*entry)();
	}
}

} // namespace cock::user
