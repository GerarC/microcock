#ifndef CXA_GUARDS_HPP
#define CXA_GUARDS_HPP
#include <stdint.h>

extern "C" {

int __cxa_guard_acquire(uint64_t *guard_object);
void __cxa_guard_release(uint64_t *guard_object);
void __cxa_guard_abort(uint64_t *guard_object);

}

#endif // !CXA_GUARDS_HPP
