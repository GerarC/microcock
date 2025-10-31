#include <cxa/cxa_atexit.hpp>
#include <stddef.h>

extern "C" {

int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle) {
	(void)func;
	(void)arg;
	(void)dso_handle;
	return 0;
}

int atexit(void (*func)(void *), void *arg, void *dso_handle) {
	(void)func;
	(void)arg;
	(void)dso_handle;
	return 0;
}
}
