#include <cxa/cxa_guards.hpp>
#include <stdint.h>

extern "C" {

int __cxa_guard_acquire(uint64_t *guard_object) {
	if (*guard_object) return 0;
	return 1;
}

void __cxa_guard_release(uint64_t *guard_object) { *guard_object = 1; }

void __cxa_guard_abort(uint64_t *guard_object) { (void)guard_object; }
}
