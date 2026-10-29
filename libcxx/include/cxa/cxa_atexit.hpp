/**
 * @file cxa_atexit.hpp
 * @brief Registration of exit handlers required by the Itanium C++ ABI.
 *
 * Deviation: the kernel and the user programs never run static destructors
 * (they never exit through `exit`), so both functions accept and discard the
 * handler.
 */
#ifndef CXA_ATEXIT_HPP
#define CXA_ATEXIT_HPP
extern "C" {

/**
 * @brief Compiler hook that registers a destructor of a static object.
 * @param func Destructor to call at exit; ignored.
 * @param arg Argument for `func`; ignored.
 * @param dso_handle Identifier of the shared object; ignored.
 * @return Always 0 (success).
 */
int __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle);

/**
 * @brief ISO C handler registration.
 * @param func Handler; ignored.
 * @return Always 0 (success). The handler is never called.
 */
int atexit(void (*func)(void));

}

#endif // !CXA_ATEXIT_HPP
