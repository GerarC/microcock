#include <cock/user/api.hpp>
#include <cock/user/constructors.hpp>
#include <cock/user/crt0.hpp>

extern "C" [[noreturn]] void _start() {
	cock::user::runConstructors();
	cock::user::exit_process(static_cast<uintptr_t>(main()));
}
