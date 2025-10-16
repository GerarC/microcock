#ifndef CXA_ATEXIT_HPP
#define CXA_ATEXIT_HPP
extern "C" {

int __cxa_atexit(void (*func)(void*), void* arg, void* dso_handle);
int atexit(void (*func)(void*), void* arg, void* dso_handle);

}

#endif // !CXA_ATEXIT_HPP

