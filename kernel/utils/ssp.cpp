#include <cock/utils/panic.hpp>
#include <cock/utils/ssp.hpp>

using cock::utils::panic;

uintptr_t __stack_chk_guard = STACK_CHK_GUARD;

extern "C" __attribute__((noreturn)) void __stack_chk_fail(void) {
#if __STDC_HOSTED__
	abort();
#elif __is_cock_kernel
	panic("Stack smashing detected");
#endif
	for (;;)
		__asm__ volatile("cli; hlt");
}
