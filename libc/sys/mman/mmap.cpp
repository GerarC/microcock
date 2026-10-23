#include <sys/mman.h>
#include <sys/syscalls.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, size_t offset) {
    uintptr_t ret = syscall(SYS_MMAP, (uintptr_t)addr, (uintptr_t)length, (uintptr_t)prot, (uintptr_t)flags, (uintptr_t)fd, (uintptr_t)offset);
    return (void*)ret;
}
