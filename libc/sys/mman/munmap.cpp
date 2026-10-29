#include <sys/mman.h>
#include <sys/syscalls.h>

int munmap(void *addr, size_t length) {
	uintptr_t result = syscall(SYS_MUNMAP, (uintptr_t)addr, (uintptr_t)length,
							   0, 0, 0, 0);
	return result == 0 ? 0 : -1;
}
