#include <cxa/cxa_guards.hpp>
#include <stdint.h>

extern "C" {

int __cxa_guard_acquire(uint64_t *guard) {
	if (*guard) return 0;
    *guard = 1;
	return 1;
}

void __cxa_guard_release(uint64_t *guard) { *guard = 1; }

void __cxa_guard_abort(uint64_t *guard) { (void)guard; }
}
