#include <cock/user/constructors.hpp>

using Constructor = void (*)();

extern "C" Constructor __init_array_start[];
extern "C" Constructor __init_array_end[];
extern "C" Constructor __ctors_start[];
extern "C" Constructor __ctors_end[];

namespace cock::user {

void runConstructors() {
	for (Constructor *entry = __init_array_start; entry < __init_array_end; entry++) {
		(*entry)();
	}
	for (Constructor *entry = __ctors_end; entry > __ctors_start;) {
		entry--;
		(*entry)();
	}
}

} // namespace cock::user
