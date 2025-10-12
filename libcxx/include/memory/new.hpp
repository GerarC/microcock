#ifndef NEW_HPP
#define NEW_HPP

#include <stddef.h>


extern "C++" {

void* operator new(size_t size);
void* operator new[](size_t size);

void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;

void operator delete(void* ptr, size_t size) noexcept;
void operator delete[](void* ptr, size_t size) noexcept;

} 
#endif // !NEW_HPP
