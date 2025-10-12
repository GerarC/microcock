#include <cock/utils/panic.hpp>
#include <memory/new.hpp>

using cock::utils::panic;

extern "C++" {

void* operator new(size_t size) {
    (void)size;
    // TODO: New not implemented
    panic("operator new() not implemented");
}

void* operator new[](size_t size) {
    (void)size;
    panic("operator new[]() not implemented");
}

void operator delete(void* ptr) noexcept {
    (void)ptr;
}

void operator delete[](void* ptr) noexcept {
    (void)ptr;
}

void operator delete(void* ptr, size_t) noexcept {
    (void)ptr;
}

void operator delete[](void* ptr, size_t) noexcept {
    (void)ptr;
}

}

