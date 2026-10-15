#include <sys/syscalls.h>
#include <unistd.h>

__attribute__((__noreturn__)) void _exit(int status) {
	syscall(SYS_EXIT, (uintptr_t)status, 0, 0, 0, 0, 0);
	__builtin_trap();
}
