#include <stddef.h>
#include <stdlib.h>

namespace {

void *allocateOrTrap(size_t size) {
	void *memory = malloc(size);
	if (!memory) abort();
	return memory;
}

} // namespace

void *operator new(size_t size) { return allocateOrTrap(size); }

void *operator new[](size_t size) { return allocateOrTrap(size); }

void operator delete(void *pointer) noexcept { free(pointer); }

void operator delete[](void *pointer) noexcept { free(pointer); }

void operator delete(void *pointer, size_t) noexcept { free(pointer); }

void operator delete[](void *pointer, size_t) noexcept { free(pointer); }
