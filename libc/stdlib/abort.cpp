#include <stdlib.h>

#if defined(__is_libk)
#include <cock/utils/macros.hpp>
#include <cock/utils/panic.hpp>
#else
#include <unistd.h>
#endif

#if defined(__is_libk)
using cock::utils::panic;
#else
namespace {

constexpr int DEBUG_OUTPUT = 1;
constexpr char ABORT_MESSAGE[] = "cock abort()\n";
constexpr size_t ABORT_MESSAGE_LENGTH = sizeof(ABORT_MESSAGE) - sizeof(char);
constexpr int SIGNAL_EXIT_BASE = 128;
constexpr int SIGABRT_NUMBER = 6;
constexpr int ABORT_EXIT_STATUS = SIGNAL_EXIT_BASE + SIGABRT_NUMBER;

} // namespace
#endif

__attribute__((__noreturn__)) void abort(void) {
#if defined(__is_libk)
	panic("abort()\n");
	asm volatile("hlt");
	FOR_ETERNAL;
	__builtin_unreachable();
#else
	write(DEBUG_OUTPUT, ABORT_MESSAGE, ABORT_MESSAGE_LENGTH);
	_exit(ABORT_EXIT_STATUS);
#endif
}
