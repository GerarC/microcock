#include <cock/abi/exec.hpp>
#include <cock/exec/args.hpp>
#include <cock/user/api.hpp>
#include <cock/user/constructors.hpp>
#include <cock/user/crt0.hpp>

extern "C" void __descriptors_import(const cock::abi::exec::ArgsBlock *block) __attribute__((weak));

namespace {

uint32_t initialHandleValue = 0;
const cock::abi::exec::ArgsBlock *arguments = nullptr;

bool takeArguments(uint32_t value) {
	if (value != cock::abi::exec::ARGS_ADDRESS) return false;

	const cock::abi::exec::ArgsBlock *block =
		reinterpret_cast<const cock::abi::exec::ArgsBlock *>(static_cast<uintptr_t>(value));
	if (!cock::exec::valid(*block)) return false;

	arguments = block;
	initialHandleValue = block->initialHandle;
	if (__descriptors_import != nullptr) __descriptors_import(block);
	return true;
}

} // namespace

namespace cock::user {

uint32_t initialHandle() { return initialHandleValue; }

uint32_t argumentCount() { return arguments != nullptr ? arguments->count : 0; }

const char *argument(uint32_t index) {
	return arguments != nullptr ? cock::exec::argument(*arguments, index) : nullptr;
}

} // namespace cock::user

extern "C" [[noreturn]] void startProgram(uint32_t handle) {
	if (!takeArguments(handle)) initialHandleValue = handle;
	cock::user::runConstructors();
	cock::user::exit_process(static_cast<uintptr_t>(main()));
}

extern "C" [[noreturn, gnu::naked]] void _start() {
	asm volatile("pushl %eax\n\tcall startProgram");
}
