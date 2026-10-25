#include <cock/user/api.hpp>
#include <cock/user/constructors.hpp>
#include <cock/user/crt0.hpp>

namespace {

uint32_t initialHandleValue = 0;

} // namespace

namespace cock::user {

uint32_t initialHandle() { return initialHandleValue; }

} // namespace cock::user

extern "C" [[noreturn]] void startProgram(uint32_t handle) {
	initialHandleValue = handle;
	cock::user::runConstructors();
	cock::user::exit_process(static_cast<uintptr_t>(main()));
}

extern "C" [[noreturn, gnu::naked]] void _start() {
	asm volatile("pushl %eax\n\tcall startProgram");
}
