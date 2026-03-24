#include "cock/core/memory/heap.hpp"
#include <cock/utils/panic.hpp>
#include <memory/new.hpp>

using cock::core::memory::kfree;
using cock::core::memory::kmalloc;
using cock::utils::panic;

extern "C++" {

void *operator new(size_t size) {
	void *ptr = kmalloc(size);
	if (!ptr) panic("operator new() out of memory");
	return ptr;
}

void *operator new[](size_t size) {
	void *ptr = kmalloc(size);
	if (!ptr) panic("operator new() out of memory");
	return ptr;
}

void operator delete(void *ptr) noexcept { kfree(ptr); }

void operator delete[](void *ptr) noexcept { kfree(ptr); }

void operator delete(void *ptr, size_t) noexcept { kfree(ptr); }

void operator delete[](void *ptr, size_t) noexcept { kfree(ptr); }
}
