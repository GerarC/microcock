/**
 * @file new.hpp
 * @brief Global `operator new` and `operator delete`.
 *
 * In the kernel they use the kernel heap and panic when it is exhausted; in
 * user space they use `malloc` and call `abort` on failure. Deviation from
 * ISO C++: they never return null and never throw (exceptions are disabled),
 * so there is no `std::nothrow` form and no `std::bad_alloc`. No aligned or
 * placement forms are declared here.
 */
#ifndef NEW_HPP
#define NEW_HPP

#include <stddef.h>


extern "C++" {

/**
 * @brief Allocates one object.
 * @param size Bytes requested.
 * @return A non-null block; the program stops when memory runs out.
 */
void* operator new(size_t size);

/**
 * @brief Allocates an array of objects.
 * @param size Bytes requested.
 * @return A non-null block; the program stops when memory runs out.
 */
void* operator new[](size_t size);

/**
 * @brief Releases memory from `operator new`.
 * @param ptr Block to release, or null (no effect).
 */
void operator delete(void* ptr) noexcept;

/**
 * @brief Releases memory from `operator new[]`.
 * @param ptr Block to release, or null (no effect).
 */
void operator delete[](void* ptr) noexcept;

/**
 * @brief Sized release of an object.
 * @param ptr Block to release, or null.
 * @param size Size given to the allocation; not used.
 */
void operator delete(void* ptr, size_t size) noexcept;

/**
 * @brief Sized release of an array.
 * @param ptr Block to release, or null.
 * @param size Size given to the allocation; not used.
 */
void operator delete[](void* ptr, size_t size) noexcept;

} 
#endif // !NEW_HPP
