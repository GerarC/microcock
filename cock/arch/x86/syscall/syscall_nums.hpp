#ifndef X86_COCK_SYSCALL_NUMS_HPP
#define X86_COCK_SYSCALL_NUMS_HPP

/**
 * @file
 * @brief System call numbers of the i386 ABI.
 *
 * ABI: Ring 3 loads the number in EAX and up to six arguments in EBX, ECX,
 * EDX, ESI, EDI and EBP, then executes `int 0x80`; the result comes back in
 * EAX. Every argument is untrusted and validated by the kernel.
 *
 * Numbers come in two families that never overlap:
 * - The Linux i386 numbers, 0 to 439, listed here with the Linux name of each
 *   call (source: the Linux i386 table). A call of ours with the name and the
 *   purpose of a Linux call has the Linux number: exit (1), read (3), write (4),
 *   getpid (20), mmap (90), munmap (91), sched_yield (158) and gettid (224).
 *   Every other Linux call is listed as not implemented: it dispatches to
 *   `sys_not_implemented`, which fails, so the number is reserved for the Linux
 *   personality server of the roadmap and never reused for something else.
 * - The microkernel calls start at 0x200, above every Linux i386 number, in
 *   blocks of 16 by purpose: processes and threads (0x200), time and
 *   synchronization (0x210), asynchronous messages (0x220), synchronous calls
 *   (0x230), capabilities (0x240), hardware access (0x250), shared memory
 *   (0x260) and boot modules (0x270). A new call goes at the end of the block of
 *   its purpose; when a block is full, the next free block is used.
 *
 * Every number that has no enumerator, and every number past the table, also
 * dispatches to `sys_not_implemented`.
 */

#include <stdint.h>

namespace cock::arch::x86 {

/**
 * @brief Identifier of each system call, stored in EAX before `int 0x80`.
 */
enum SyscallNum : uint32_t {
    SYS_RESTART_SYSCALL          = 0x000, /**< Linux `restart_syscall`: not implemented; the call fails and nothing happens. */
    SYS_EXIT                     = 0x001, /**< Terminate the calling process and record its exit status. */
    SYS_FORK                     = 0x002, /**< Linux `fork`: not implemented; the call fails and nothing happens. */
    SYS_READ                     = 0x003, /**< Read from standard input (file descriptor 0 only; the kernel has no input queue, so it validates the buffer and reports end of input with 0; input comes from the console server). */
    SYS_WRITE                    = 0x004, /**< Write at most 128 bytes per call to the console (file descriptor 1 or 2), forwarded to the display server or, without one, to the kernel console; returns the bytes accepted. After 1000 restarts against a full display server inbox it writes to the kernel console instead. */
    SYS_OPEN                     = 0x005, /**< Linux `open`: not implemented; the call fails and nothing happens. */
    SYS_CLOSE                    = 0x006, /**< Linux `close`: not implemented; the call fails and nothing happens. */
    SYS_WAITPID                  = 0x007, /**< Linux `waitpid`: not implemented; the call fails and nothing happens. */
    SYS_CREAT                    = 0x008, /**< Linux `creat`: not implemented; the call fails and nothing happens. */
    SYS_LINK                     = 0x009, /**< Linux `link`: not implemented; the call fails and nothing happens. */
    SYS_UNLINK                   = 0x00A, /**< Linux `unlink`: not implemented; the call fails and nothing happens. */
    SYS_EXECVE                   = 0x00B, /**< Linux `execve`: not implemented; the call fails and nothing happens. */
    SYS_CHDIR                    = 0x00C, /**< Linux `chdir`: not implemented; the call fails and nothing happens. */
    SYS_TIME                     = 0x00D, /**< Linux `time`: not implemented; the call fails and nothing happens. */
    SYS_MKNOD                    = 0x00E, /**< Linux `mknod`: not implemented; the call fails and nothing happens. */
    SYS_CHMOD                    = 0x00F, /**< Linux `chmod`: not implemented; the call fails and nothing happens. */
    SYS_LCHOWN                   = 0x010, /**< Linux `lchown`: not implemented; the call fails and nothing happens. */
    SYS_BREAK                    = 0x011, /**< Linux `break`: not implemented; the call fails and nothing happens. */
    SYS_OLDSTAT                  = 0x012, /**< Linux `oldstat`: not implemented; the call fails and nothing happens. */
    SYS_LSEEK                    = 0x013, /**< Linux `lseek`: not implemented; the call fails and nothing happens. */
    SYS_GETPID                   = 0x014, /**< Get the id of the calling process. */
    SYS_MOUNT                    = 0x015, /**< Linux `mount`: not implemented; the call fails and nothing happens. */
    SYS_UMOUNT                   = 0x016, /**< Linux `umount`: not implemented; the call fails and nothing happens. */
    SYS_SETUID                   = 0x017, /**< Linux `setuid`: not implemented; the call fails and nothing happens. */
    SYS_GETUID                   = 0x018, /**< Linux `getuid`: not implemented; the call fails and nothing happens. */
    SYS_STIME                    = 0x019, /**< Linux `stime`: not implemented; the call fails and nothing happens. */
    SYS_PTRACE                   = 0x01A, /**< Linux `ptrace`: not implemented; the call fails and nothing happens. */
    SYS_ALARM                    = 0x01B, /**< Linux `alarm`: not implemented; the call fails and nothing happens. */
    SYS_OLDFSTAT                 = 0x01C, /**< Linux `oldfstat`: not implemented; the call fails and nothing happens. */
    SYS_PAUSE                    = 0x01D, /**< Linux `pause`: not implemented; the call fails and nothing happens. */
    SYS_UTIME                    = 0x01E, /**< Linux `utime`: not implemented; the call fails and nothing happens. */
    SYS_STTY                     = 0x01F, /**< Linux `stty`: not implemented; the call fails and nothing happens. */
    SYS_GTTY                     = 0x020, /**< Linux `gtty`: not implemented; the call fails and nothing happens. */
    SYS_ACCESS                   = 0x021, /**< Linux `access`: not implemented; the call fails and nothing happens. */
    SYS_NICE                     = 0x022, /**< Linux `nice`: not implemented; the call fails and nothing happens. */
    SYS_FTIME                    = 0x023, /**< Linux `ftime`: not implemented; the call fails and nothing happens. */
    SYS_SYNC                     = 0x024, /**< Linux `sync`: not implemented; the call fails and nothing happens. */
    SYS_KILL                     = 0x025, /**< Linux `kill`: not implemented; the call fails and nothing happens. */
    SYS_RENAME                   = 0x026, /**< Linux `rename`: not implemented; the call fails and nothing happens. */
    SYS_MKDIR                    = 0x027, /**< Linux `mkdir`: not implemented; the call fails and nothing happens. */
    SYS_RMDIR                    = 0x028, /**< Linux `rmdir`: not implemented; the call fails and nothing happens. */
    SYS_DUP                      = 0x029, /**< Linux `dup`: not implemented; the call fails and nothing happens. */
    SYS_PIPE                     = 0x02A, /**< Linux `pipe`: not implemented; the call fails and nothing happens. */
    SYS_TIMES                    = 0x02B, /**< Linux `times`: not implemented; the call fails and nothing happens. */
    SYS_PROF                     = 0x02C, /**< Linux `prof`: not implemented; the call fails and nothing happens. */
    SYS_BRK                      = 0x02D, /**< Linux `brk`: not implemented; the call fails and nothing happens. */
    SYS_SETGID                   = 0x02E, /**< Linux `setgid`: not implemented; the call fails and nothing happens. */
    SYS_GETGID                   = 0x02F, /**< Linux `getgid`: not implemented; the call fails and nothing happens. */
    SYS_SIGNAL                   = 0x030, /**< Linux `signal`: not implemented; the call fails and nothing happens. */
    SYS_GETEUID                  = 0x031, /**< Linux `geteuid`: not implemented; the call fails and nothing happens. */
    SYS_GETEGID                  = 0x032, /**< Linux `getegid`: not implemented; the call fails and nothing happens. */
    SYS_ACCT                     = 0x033, /**< Linux `acct`: not implemented; the call fails and nothing happens. */
    SYS_UMOUNT2                  = 0x034, /**< Linux `umount2`: not implemented; the call fails and nothing happens. */
    SYS_LOCK                     = 0x035, /**< Linux `lock`: not implemented; the call fails and nothing happens. */
    SYS_IOCTL                    = 0x036, /**< Linux `ioctl`: not implemented; the call fails and nothing happens. */
    SYS_FCNTL                    = 0x037, /**< Linux `fcntl`: not implemented; the call fails and nothing happens. */
    SYS_MPX                      = 0x038, /**< Linux `mpx`: not implemented; the call fails and nothing happens. */
    SYS_SETPGID                  = 0x039, /**< Linux `setpgid`: not implemented; the call fails and nothing happens. */
    SYS_ULIMIT                   = 0x03A, /**< Linux `ulimit`: not implemented; the call fails and nothing happens. */
    SYS_OLDOLDUNAME              = 0x03B, /**< Linux `oldolduname`: not implemented; the call fails and nothing happens. */
    SYS_UMASK                    = 0x03C, /**< Linux `umask`: not implemented; the call fails and nothing happens. */
    SYS_CHROOT                   = 0x03D, /**< Linux `chroot`: not implemented; the call fails and nothing happens. */
    SYS_USTAT                    = 0x03E, /**< Linux `ustat`: not implemented; the call fails and nothing happens. */
    SYS_DUP2                     = 0x03F, /**< Linux `dup2`: not implemented; the call fails and nothing happens. */
    SYS_GETPPID                  = 0x040, /**< Linux `getppid`: not implemented; the call fails and nothing happens. */
    SYS_GETPGRP                  = 0x041, /**< Linux `getpgrp`: not implemented; the call fails and nothing happens. */
    SYS_SETSID                   = 0x042, /**< Linux `setsid`: not implemented; the call fails and nothing happens. */
    SYS_SIGACTION                = 0x043, /**< Linux `sigaction`: not implemented; the call fails and nothing happens. */
    SYS_SGETMASK                 = 0x044, /**< Linux `sgetmask`: not implemented; the call fails and nothing happens. */
    SYS_SSETMASK                 = 0x045, /**< Linux `ssetmask`: not implemented; the call fails and nothing happens. */
    SYS_SETREUID                 = 0x046, /**< Linux `setreuid`: not implemented; the call fails and nothing happens. */
    SYS_SETREGID                 = 0x047, /**< Linux `setregid`: not implemented; the call fails and nothing happens. */
    SYS_SIGSUSPEND               = 0x048, /**< Linux `sigsuspend`: not implemented; the call fails and nothing happens. */
    SYS_SIGPENDING               = 0x049, /**< Linux `sigpending`: not implemented; the call fails and nothing happens. */
    SYS_SETHOSTNAME              = 0x04A, /**< Linux `sethostname`: not implemented; the call fails and nothing happens. */
    SYS_SETRLIMIT                = 0x04B, /**< Linux `setrlimit`: not implemented; the call fails and nothing happens. */
    SYS_GETRLIMIT                = 0x04C, /**< Linux `getrlimit`: not implemented; the call fails and nothing happens. */
    SYS_GETRUSAGE                = 0x04D, /**< Linux `getrusage`: not implemented; the call fails and nothing happens. */
    SYS_GETTIMEOFDAY             = 0x04E, /**< Linux `gettimeofday`: not implemented; the call fails and nothing happens. */
    SYS_SETTIMEOFDAY             = 0x04F, /**< Linux `settimeofday`: not implemented; the call fails and nothing happens. */
    SYS_GETGROUPS                = 0x050, /**< Linux `getgroups`: not implemented; the call fails and nothing happens. */
    SYS_SETGROUPS                = 0x051, /**< Linux `setgroups`: not implemented; the call fails and nothing happens. */
    SYS_SELECT                   = 0x052, /**< Linux `select`: not implemented; the call fails and nothing happens. */
    SYS_SYMLINK                  = 0x053, /**< Linux `symlink`: not implemented; the call fails and nothing happens. */
    SYS_OLDLSTAT                 = 0x054, /**< Linux `oldlstat`: not implemented; the call fails and nothing happens. */
    SYS_READLINK                 = 0x055, /**< Linux `readlink`: not implemented; the call fails and nothing happens. */
    SYS_USELIB                   = 0x056, /**< Linux `uselib`: not implemented; the call fails and nothing happens. */
    SYS_SWAPON                   = 0x057, /**< Linux `swapon`: not implemented; the call fails and nothing happens. */
    SYS_REBOOT                   = 0x058, /**< Linux `reboot`: not implemented; the call fails and nothing happens. */
    SYS_READDIR                  = 0x059, /**< Linux `readdir`: not implemented; the call fails and nothing happens. */
    SYS_MMAP                     = 0x05A, /**< Map fresh anonymous pages into the caller, or (with MAP_PHYS and a PHYSICAL_MEMORY capability) device memory at a fixed address. */
    SYS_MUNMAP                   = 0x05B, /**< Unmap a whole region previously mapped (same start address and page count). */
    SYS_TRUNCATE                 = 0x05C, /**< Linux `truncate`: not implemented; the call fails and nothing happens. */
    SYS_FTRUNCATE                = 0x05D, /**< Linux `ftruncate`: not implemented; the call fails and nothing happens. */
    SYS_FCHMOD                   = 0x05E, /**< Linux `fchmod`: not implemented; the call fails and nothing happens. */
    SYS_FCHOWN                   = 0x05F, /**< Linux `fchown`: not implemented; the call fails and nothing happens. */
    SYS_GETPRIORITY              = 0x060, /**< Linux `getpriority`: not implemented; the call fails and nothing happens. */
    SYS_SETPRIORITY              = 0x061, /**< Linux `setpriority`: not implemented; the call fails and nothing happens. */
    SYS_PROFIL                   = 0x062, /**< Linux `profil`: not implemented; the call fails and nothing happens. */
    SYS_STATFS                   = 0x063, /**< Linux `statfs`: not implemented; the call fails and nothing happens. */
    SYS_FSTATFS                  = 0x064, /**< Linux `fstatfs`: not implemented; the call fails and nothing happens. */
    SYS_IOPERM                   = 0x065, /**< Linux `ioperm`: not implemented; the call fails and nothing happens. */
    SYS_SOCKETCALL               = 0x066, /**< Linux `socketcall`: not implemented; the call fails and nothing happens. */
    SYS_SYSLOG                   = 0x067, /**< Linux `syslog`: not implemented; the call fails and nothing happens. */
    SYS_SETITIMER                = 0x068, /**< Linux `setitimer`: not implemented; the call fails and nothing happens. */
    SYS_GETITIMER                = 0x069, /**< Linux `getitimer`: not implemented; the call fails and nothing happens. */
    SYS_STAT                     = 0x06A, /**< Linux `stat`: not implemented; the call fails and nothing happens. */
    SYS_LSTAT                    = 0x06B, /**< Linux `lstat`: not implemented; the call fails and nothing happens. */
    SYS_FSTAT                    = 0x06C, /**< Linux `fstat`: not implemented; the call fails and nothing happens. */
    SYS_OLDUNAME                 = 0x06D, /**< Linux `olduname`: not implemented; the call fails and nothing happens. */
    SYS_IOPL                     = 0x06E, /**< Linux `iopl`: not implemented; the call fails and nothing happens. */
    SYS_VHANGUP                  = 0x06F, /**< Linux `vhangup`: not implemented; the call fails and nothing happens. */
    SYS_IDLE                     = 0x070, /**< Linux `idle`: not implemented; the call fails and nothing happens. */
    SYS_VM86OLD                  = 0x071, /**< Linux `vm86old`: not implemented; the call fails and nothing happens. */
    SYS_WAIT4                    = 0x072, /**< Linux `wait4`: not implemented; the call fails and nothing happens. */
    SYS_SWAPOFF                  = 0x073, /**< Linux `swapoff`: not implemented; the call fails and nothing happens. */
    SYS_SYSINFO                  = 0x074, /**< Linux `sysinfo`: not implemented; the call fails and nothing happens. */
    SYS_IPC                      = 0x075, /**< Linux `ipc`: not implemented; the call fails and nothing happens. */
    SYS_FSYNC                    = 0x076, /**< Linux `fsync`: not implemented; the call fails and nothing happens. */
    SYS_SIGRETURN                = 0x077, /**< Linux `sigreturn`: not implemented; the call fails and nothing happens. */
    SYS_CLONE                    = 0x078, /**< Linux `clone`: not implemented; the call fails and nothing happens. */
    SYS_SETDOMAINNAME            = 0x079, /**< Linux `setdomainname`: not implemented; the call fails and nothing happens. */
    SYS_UNAME                    = 0x07A, /**< Linux `uname`: not implemented; the call fails and nothing happens. */
    SYS_MODIFY_LDT               = 0x07B, /**< Linux `modify_ldt`: not implemented; the call fails and nothing happens. */
    SYS_ADJTIMEX                 = 0x07C, /**< Linux `adjtimex`: not implemented; the call fails and nothing happens. */
    SYS_MPROTECT                 = 0x07D, /**< Linux `mprotect`: not implemented; the call fails and nothing happens. */
    SYS_SIGPROCMASK              = 0x07E, /**< Linux `sigprocmask`: not implemented; the call fails and nothing happens. */
    SYS_CREATE_MODULE            = 0x07F, /**< Linux `create_module`: not implemented; the call fails and nothing happens. */
    SYS_INIT_MODULE              = 0x080, /**< Linux `init_module`: not implemented; the call fails and nothing happens. */
    SYS_DELETE_MODULE            = 0x081, /**< Linux `delete_module`: not implemented; the call fails and nothing happens. */
    SYS_GET_KERNEL_SYMS          = 0x082, /**< Linux `get_kernel_syms`: not implemented; the call fails and nothing happens. */
    SYS_QUOTACTL                 = 0x083, /**< Linux `quotactl`: not implemented; the call fails and nothing happens. */
    SYS_GETPGID                  = 0x084, /**< Linux `getpgid`: not implemented; the call fails and nothing happens. */
    SYS_FCHDIR                   = 0x085, /**< Linux `fchdir`: not implemented; the call fails and nothing happens. */
    SYS_BDFLUSH                  = 0x086, /**< Linux `bdflush`: not implemented; the call fails and nothing happens. */
    SYS_SYSFS                    = 0x087, /**< Linux `sysfs`: not implemented; the call fails and nothing happens. */
    SYS_PERSONALITY              = 0x088, /**< Linux `personality`: not implemented; the call fails and nothing happens. */
    SYS_AFS_SYSCALL              = 0x089, /**< Linux `afs_syscall`: not implemented; the call fails and nothing happens. */
    SYS_SETFSUID                 = 0x08A, /**< Linux `setfsuid`: not implemented; the call fails and nothing happens. */
    SYS_SETFSGID                 = 0x08B, /**< Linux `setfsgid`: not implemented; the call fails and nothing happens. */
    SYS_LLSEEK                   = 0x08C, /**< Linux `_llseek`: not implemented; the call fails and nothing happens. */
    SYS_GETDENTS                 = 0x08D, /**< Linux `getdents`: not implemented; the call fails and nothing happens. */
    SYS_NEWSELECT                = 0x08E, /**< Linux `_newselect`: not implemented; the call fails and nothing happens. */
    SYS_FLOCK                    = 0x08F, /**< Linux `flock`: not implemented; the call fails and nothing happens. */
    SYS_MSYNC                    = 0x090, /**< Linux `msync`: not implemented; the call fails and nothing happens. */
    SYS_READV                    = 0x091, /**< Linux `readv`: not implemented; the call fails and nothing happens. */
    SYS_WRITEV                   = 0x092, /**< Linux `writev`: not implemented; the call fails and nothing happens. */
    SYS_GETSID                   = 0x093, /**< Linux `getsid`: not implemented; the call fails and nothing happens. */
    SYS_FDATASYNC                = 0x094, /**< Linux `fdatasync`: not implemented; the call fails and nothing happens. */
    SYS_SYSCTL                   = 0x095, /**< Linux `_sysctl`: not implemented; the call fails and nothing happens. */
    SYS_MLOCK                    = 0x096, /**< Linux `mlock`: not implemented; the call fails and nothing happens. */
    SYS_MUNLOCK                  = 0x097, /**< Linux `munlock`: not implemented; the call fails and nothing happens. */
    SYS_MLOCKALL                 = 0x098, /**< Linux `mlockall`: not implemented; the call fails and nothing happens. */
    SYS_MUNLOCKALL               = 0x099, /**< Linux `munlockall`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_SETPARAM           = 0x09A, /**< Linux `sched_setparam`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_GETPARAM           = 0x09B, /**< Linux `sched_getparam`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_SETSCHEDULER       = 0x09C, /**< Linux `sched_setscheduler`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_GETSCHEDULER       = 0x09D, /**< Linux `sched_getscheduler`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_YIELD              = 0x09E, /**< Give up the CPU voluntarily. */
    SYS_SCHED_GET_PRIORITY_MAX   = 0x09F, /**< Linux `sched_get_priority_max`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_GET_PRIORITY_MIN   = 0x0A0, /**< Linux `sched_get_priority_min`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_RR_GET_INTERVAL    = 0x0A1, /**< Linux `sched_rr_get_interval`: not implemented; the call fails and nothing happens. */
    SYS_NANOSLEEP                = 0x0A2, /**< Linux `nanosleep`: not implemented; the call fails and nothing happens. */
    SYS_MREMAP                   = 0x0A3, /**< Linux `mremap`: not implemented; the call fails and nothing happens. */
    SYS_SETRESUID                = 0x0A4, /**< Linux `setresuid`: not implemented; the call fails and nothing happens. */
    SYS_GETRESUID                = 0x0A5, /**< Linux `getresuid`: not implemented; the call fails and nothing happens. */
    SYS_VM86                     = 0x0A6, /**< Linux `vm86`: not implemented; the call fails and nothing happens. */
    SYS_QUERY_MODULE             = 0x0A7, /**< Linux `query_module`: not implemented; the call fails and nothing happens. */
    SYS_POLL                     = 0x0A8, /**< Linux `poll`: not implemented; the call fails and nothing happens. */
    SYS_NFSSERVCTL               = 0x0A9, /**< Linux `nfsservctl`: not implemented; the call fails and nothing happens. */
    SYS_SETRESGID                = 0x0AA, /**< Linux `setresgid`: not implemented; the call fails and nothing happens. */
    SYS_GETRESGID                = 0x0AB, /**< Linux `getresgid`: not implemented; the call fails and nothing happens. */
    SYS_PRCTL                    = 0x0AC, /**< Linux `prctl`: not implemented; the call fails and nothing happens. */
    SYS_RT_SIGRETURN             = 0x0AD, /**< Linux `rt_sigreturn`: not implemented; the call fails and nothing happens. */
    SYS_RT_SIGACTION             = 0x0AE, /**< Linux `rt_sigaction`: not implemented; the call fails and nothing happens. */
    SYS_RT_SIGPROCMASK           = 0x0AF, /**< Linux `rt_sigprocmask`: not implemented; the call fails and nothing happens. */
    SYS_RT_SIGPENDING            = 0x0B0, /**< Linux `rt_sigpending`: not implemented; the call fails and nothing happens. */
    SYS_RT_SIGTIMEDWAIT          = 0x0B1, /**< Linux `rt_sigtimedwait`: not implemented; the call fails and nothing happens. */
    SYS_RT_SIGQUEUEINFO          = 0x0B2, /**< Linux `rt_sigqueueinfo`: not implemented; the call fails and nothing happens. */
    SYS_RT_SIGSUSPEND            = 0x0B3, /**< Linux `rt_sigsuspend`: not implemented; the call fails and nothing happens. */
    SYS_PREAD64                  = 0x0B4, /**< Linux `pread64`: not implemented; the call fails and nothing happens. */
    SYS_PWRITE64                 = 0x0B5, /**< Linux `pwrite64`: not implemented; the call fails and nothing happens. */
    SYS_CHOWN                    = 0x0B6, /**< Linux `chown`: not implemented; the call fails and nothing happens. */
    SYS_GETCWD                   = 0x0B7, /**< Linux `getcwd`: not implemented; the call fails and nothing happens. */
    SYS_CAPGET                   = 0x0B8, /**< Linux `capget`: not implemented; the call fails and nothing happens. */
    SYS_CAPSET                   = 0x0B9, /**< Linux `capset`: not implemented; the call fails and nothing happens. */
    SYS_SIGALTSTACK              = 0x0BA, /**< Linux `sigaltstack`: not implemented; the call fails and nothing happens. */
    SYS_SENDFILE                 = 0x0BB, /**< Linux `sendfile`: not implemented; the call fails and nothing happens. */
    SYS_GETPMSG                  = 0x0BC, /**< Linux `getpmsg`: not implemented; the call fails and nothing happens. */
    SYS_PUTPMSG                  = 0x0BD, /**< Linux `putpmsg`: not implemented; the call fails and nothing happens. */
    SYS_VFORK                    = 0x0BE, /**< Linux `vfork`: not implemented; the call fails and nothing happens. */
    SYS_UGETRLIMIT               = 0x0BF, /**< Linux `ugetrlimit`: not implemented; the call fails and nothing happens. */
    SYS_MMAP2                    = 0x0C0, /**< Linux `mmap2`: not implemented; the call fails and nothing happens. */
    SYS_TRUNCATE64               = 0x0C1, /**< Linux `truncate64`: not implemented; the call fails and nothing happens. */
    SYS_FTRUNCATE64              = 0x0C2, /**< Linux `ftruncate64`: not implemented; the call fails and nothing happens. */
    SYS_STAT64                   = 0x0C3, /**< Linux `stat64`: not implemented; the call fails and nothing happens. */
    SYS_LSTAT64                  = 0x0C4, /**< Linux `lstat64`: not implemented; the call fails and nothing happens. */
    SYS_FSTAT64                  = 0x0C5, /**< Linux `fstat64`: not implemented; the call fails and nothing happens. */
    SYS_LCHOWN32                 = 0x0C6, /**< Linux `lchown32`: not implemented; the call fails and nothing happens. */
    SYS_GETUID32                 = 0x0C7, /**< Linux `getuid32`: not implemented; the call fails and nothing happens. */
    SYS_GETGID32                 = 0x0C8, /**< Linux `getgid32`: not implemented; the call fails and nothing happens. */
    SYS_GETEUID32                = 0x0C9, /**< Linux `geteuid32`: not implemented; the call fails and nothing happens. */
    SYS_GETEGID32                = 0x0CA, /**< Linux `getegid32`: not implemented; the call fails and nothing happens. */
    SYS_SETREUID32               = 0x0CB, /**< Linux `setreuid32`: not implemented; the call fails and nothing happens. */
    SYS_SETREGID32               = 0x0CC, /**< Linux `setregid32`: not implemented; the call fails and nothing happens. */
    SYS_GETGROUPS32              = 0x0CD, /**< Linux `getgroups32`: not implemented; the call fails and nothing happens. */
    SYS_SETGROUPS32              = 0x0CE, /**< Linux `setgroups32`: not implemented; the call fails and nothing happens. */
    SYS_FCHOWN32                 = 0x0CF, /**< Linux `fchown32`: not implemented; the call fails and nothing happens. */
    SYS_SETRESUID32              = 0x0D0, /**< Linux `setresuid32`: not implemented; the call fails and nothing happens. */
    SYS_GETRESUID32              = 0x0D1, /**< Linux `getresuid32`: not implemented; the call fails and nothing happens. */
    SYS_SETRESGID32              = 0x0D2, /**< Linux `setresgid32`: not implemented; the call fails and nothing happens. */
    SYS_GETRESGID32              = 0x0D3, /**< Linux `getresgid32`: not implemented; the call fails and nothing happens. */
    SYS_CHOWN32                  = 0x0D4, /**< Linux `chown32`: not implemented; the call fails and nothing happens. */
    SYS_SETUID32                 = 0x0D5, /**< Linux `setuid32`: not implemented; the call fails and nothing happens. */
    SYS_SETGID32                 = 0x0D6, /**< Linux `setgid32`: not implemented; the call fails and nothing happens. */
    SYS_SETFSUID32               = 0x0D7, /**< Linux `setfsuid32`: not implemented; the call fails and nothing happens. */
    SYS_SETFSGID32               = 0x0D8, /**< Linux `setfsgid32`: not implemented; the call fails and nothing happens. */
    SYS_PIVOT_ROOT               = 0x0D9, /**< Linux `pivot_root`: not implemented; the call fails and nothing happens. */
    SYS_MINCORE                  = 0x0DA, /**< Linux `mincore`: not implemented; the call fails and nothing happens. */
    SYS_MADVISE                  = 0x0DB, /**< Linux `madvise`: not implemented; the call fails and nothing happens. */
    SYS_GETDENTS64               = 0x0DC, /**< Linux `getdents64`: not implemented; the call fails and nothing happens. */
    SYS_FCNTL64                  = 0x0DD, /**< Linux `fcntl64`: not implemented; the call fails and nothing happens. */
    SYS_GETTID                   = 0x0E0, /**< Get the id of the calling thread. */
    SYS_READAHEAD                = 0x0E1, /**< Linux `readahead`: not implemented; the call fails and nothing happens. */
    SYS_SETXATTR                 = 0x0E2, /**< Linux `setxattr`: not implemented; the call fails and nothing happens. */
    SYS_LSETXATTR                = 0x0E3, /**< Linux `lsetxattr`: not implemented; the call fails and nothing happens. */
    SYS_FSETXATTR                = 0x0E4, /**< Linux `fsetxattr`: not implemented; the call fails and nothing happens. */
    SYS_GETXATTR                 = 0x0E5, /**< Linux `getxattr`: not implemented; the call fails and nothing happens. */
    SYS_LGETXATTR                = 0x0E6, /**< Linux `lgetxattr`: not implemented; the call fails and nothing happens. */
    SYS_FGETXATTR                = 0x0E7, /**< Linux `fgetxattr`: not implemented; the call fails and nothing happens. */
    SYS_LISTXATTR                = 0x0E8, /**< Linux `listxattr`: not implemented; the call fails and nothing happens. */
    SYS_LLISTXATTR               = 0x0E9, /**< Linux `llistxattr`: not implemented; the call fails and nothing happens. */
    SYS_FLISTXATTR               = 0x0EA, /**< Linux `flistxattr`: not implemented; the call fails and nothing happens. */
    SYS_REMOVEXATTR              = 0x0EB, /**< Linux `removexattr`: not implemented; the call fails and nothing happens. */
    SYS_LREMOVEXATTR             = 0x0EC, /**< Linux `lremovexattr`: not implemented; the call fails and nothing happens. */
    SYS_FREMOVEXATTR             = 0x0ED, /**< Linux `fremovexattr`: not implemented; the call fails and nothing happens. */
    SYS_TKILL                    = 0x0EE, /**< Linux `tkill`: not implemented; the call fails and nothing happens. */
    SYS_SENDFILE64               = 0x0EF, /**< Linux `sendfile64`: not implemented; the call fails and nothing happens. */
    SYS_FUTEX                    = 0x0F0, /**< Linux `futex`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_SETAFFINITY        = 0x0F1, /**< Linux `sched_setaffinity`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_GETAFFINITY        = 0x0F2, /**< Linux `sched_getaffinity`: not implemented; the call fails and nothing happens. */
    SYS_SET_THREAD_AREA          = 0x0F3, /**< Linux `set_thread_area`: not implemented; the call fails and nothing happens. */
    SYS_GET_THREAD_AREA          = 0x0F4, /**< Linux `get_thread_area`: not implemented; the call fails and nothing happens. */
    SYS_IO_SETUP                 = 0x0F5, /**< Linux `io_setup`: not implemented; the call fails and nothing happens. */
    SYS_IO_DESTROY               = 0x0F6, /**< Linux `io_destroy`: not implemented; the call fails and nothing happens. */
    SYS_IO_GETEVENTS             = 0x0F7, /**< Linux `io_getevents`: not implemented; the call fails and nothing happens. */
    SYS_IO_SUBMIT                = 0x0F8, /**< Linux `io_submit`: not implemented; the call fails and nothing happens. */
    SYS_IO_CANCEL                = 0x0F9, /**< Linux `io_cancel`: not implemented; the call fails and nothing happens. */
    SYS_FADVISE64                = 0x0FA, /**< Linux `fadvise64`: not implemented; the call fails and nothing happens. */
    SYS_EXIT_GROUP               = 0x0FC, /**< Linux `exit_group`: not implemented; the call fails and nothing happens. */
    SYS_LOOKUP_DCOOKIE           = 0x0FD, /**< Linux `lookup_dcookie`: not implemented; the call fails and nothing happens. */
    SYS_EPOLL_CREATE             = 0x0FE, /**< Linux `epoll_create`: not implemented; the call fails and nothing happens. */
    SYS_EPOLL_CTL                = 0x0FF, /**< Linux `epoll_ctl`: not implemented; the call fails and nothing happens. */
    SYS_EPOLL_WAIT               = 0x100, /**< Linux `epoll_wait`: not implemented; the call fails and nothing happens. */
    SYS_REMAP_FILE_PAGES         = 0x101, /**< Linux `remap_file_pages`: not implemented; the call fails and nothing happens. */
    SYS_SET_TID_ADDRESS          = 0x102, /**< Linux `set_tid_address`: not implemented; the call fails and nothing happens. */
    SYS_TIMER_CREATE             = 0x103, /**< Linux `timer_create`: not implemented; the call fails and nothing happens. */
    SYS_TIMER_SETTIME            = 0x104, /**< Linux `timer_settime`: not implemented; the call fails and nothing happens. */
    SYS_TIMER_GETTIME            = 0x105, /**< Linux `timer_gettime`: not implemented; the call fails and nothing happens. */
    SYS_TIMER_GETOVERRUN         = 0x106, /**< Linux `timer_getoverrun`: not implemented; the call fails and nothing happens. */
    SYS_TIMER_DELETE             = 0x107, /**< Linux `timer_delete`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_SETTIME            = 0x108, /**< Linux `clock_settime`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_GETTIME            = 0x109, /**< Linux `clock_gettime`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_GETRES             = 0x10A, /**< Linux `clock_getres`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_NANOSLEEP          = 0x10B, /**< Linux `clock_nanosleep`: not implemented; the call fails and nothing happens. */
    SYS_STATFS64                 = 0x10C, /**< Linux `statfs64`: not implemented; the call fails and nothing happens. */
    SYS_FSTATFS64                = 0x10D, /**< Linux `fstatfs64`: not implemented; the call fails and nothing happens. */
    SYS_TGKILL                   = 0x10E, /**< Linux `tgkill`: not implemented; the call fails and nothing happens. */
    SYS_UTIMES                   = 0x10F, /**< Linux `utimes`: not implemented; the call fails and nothing happens. */
    SYS_FADVISE64_64             = 0x110, /**< Linux `fadvise64_64`: not implemented; the call fails and nothing happens. */
    SYS_VSERVER                  = 0x111, /**< Linux `vserver`: not implemented; the call fails and nothing happens. */
    SYS_MBIND                    = 0x112, /**< Linux `mbind`: not implemented; the call fails and nothing happens. */
    SYS_GET_MEMPOLICY            = 0x113, /**< Linux `get_mempolicy`: not implemented; the call fails and nothing happens. */
    SYS_SET_MEMPOLICY            = 0x114, /**< Linux `set_mempolicy`: not implemented; the call fails and nothing happens. */
    SYS_MQ_OPEN                  = 0x115, /**< Linux `mq_open`: not implemented; the call fails and nothing happens. */
    SYS_MQ_UNLINK                = 0x116, /**< Linux `mq_unlink`: not implemented; the call fails and nothing happens. */
    SYS_MQ_TIMEDSEND             = 0x117, /**< Linux `mq_timedsend`: not implemented; the call fails and nothing happens. */
    SYS_MQ_TIMEDRECEIVE          = 0x118, /**< Linux `mq_timedreceive`: not implemented; the call fails and nothing happens. */
    SYS_MQ_NOTIFY                = 0x119, /**< Linux `mq_notify`: not implemented; the call fails and nothing happens. */
    SYS_MQ_GETSETATTR            = 0x11A, /**< Linux `mq_getsetattr`: not implemented; the call fails and nothing happens. */
    SYS_KEXEC_LOAD               = 0x11B, /**< Linux `kexec_load`: not implemented; the call fails and nothing happens. */
    SYS_WAITID                   = 0x11C, /**< Linux `waitid`: not implemented; the call fails and nothing happens. */
    SYS_ADD_KEY                  = 0x11E, /**< Linux `add_key`: not implemented; the call fails and nothing happens. */
    SYS_REQUEST_KEY              = 0x11F, /**< Linux `request_key`: not implemented; the call fails and nothing happens. */
    SYS_KEYCTL                   = 0x120, /**< Linux `keyctl`: not implemented; the call fails and nothing happens. */
    SYS_IOPRIO_SET               = 0x121, /**< Linux `ioprio_set`: not implemented; the call fails and nothing happens. */
    SYS_IOPRIO_GET               = 0x122, /**< Linux `ioprio_get`: not implemented; the call fails and nothing happens. */
    SYS_INOTIFY_INIT             = 0x123, /**< Linux `inotify_init`: not implemented; the call fails and nothing happens. */
    SYS_INOTIFY_ADD_WATCH        = 0x124, /**< Linux `inotify_add_watch`: not implemented; the call fails and nothing happens. */
    SYS_INOTIFY_RM_WATCH         = 0x125, /**< Linux `inotify_rm_watch`: not implemented; the call fails and nothing happens. */
    SYS_MIGRATE_PAGES            = 0x126, /**< Linux `migrate_pages`: not implemented; the call fails and nothing happens. */
    SYS_OPENAT                   = 0x127, /**< Linux `openat`: not implemented; the call fails and nothing happens. */
    SYS_MKDIRAT                  = 0x128, /**< Linux `mkdirat`: not implemented; the call fails and nothing happens. */
    SYS_MKNODAT                  = 0x129, /**< Linux `mknodat`: not implemented; the call fails and nothing happens. */
    SYS_FCHOWNAT                 = 0x12A, /**< Linux `fchownat`: not implemented; the call fails and nothing happens. */
    SYS_FUTIMESAT                = 0x12B, /**< Linux `futimesat`: not implemented; the call fails and nothing happens. */
    SYS_FSTATAT64                = 0x12C, /**< Linux `fstatat64`: not implemented; the call fails and nothing happens. */
    SYS_UNLINKAT                 = 0x12D, /**< Linux `unlinkat`: not implemented; the call fails and nothing happens. */
    SYS_RENAMEAT                 = 0x12E, /**< Linux `renameat`: not implemented; the call fails and nothing happens. */
    SYS_LINKAT                   = 0x12F, /**< Linux `linkat`: not implemented; the call fails and nothing happens. */
    SYS_SYMLINKAT                = 0x130, /**< Linux `symlinkat`: not implemented; the call fails and nothing happens. */
    SYS_READLINKAT               = 0x131, /**< Linux `readlinkat`: not implemented; the call fails and nothing happens. */
    SYS_FCHMODAT                 = 0x132, /**< Linux `fchmodat`: not implemented; the call fails and nothing happens. */
    SYS_FACCESSAT                = 0x133, /**< Linux `faccessat`: not implemented; the call fails and nothing happens. */
    SYS_PSELECT6                 = 0x134, /**< Linux `pselect6`: not implemented; the call fails and nothing happens. */
    SYS_PPOLL                    = 0x135, /**< Linux `ppoll`: not implemented; the call fails and nothing happens. */
    SYS_UNSHARE                  = 0x136, /**< Linux `unshare`: not implemented; the call fails and nothing happens. */
    SYS_SET_ROBUST_LIST          = 0x137, /**< Linux `set_robust_list`: not implemented; the call fails and nothing happens. */
    SYS_GET_ROBUST_LIST          = 0x138, /**< Linux `get_robust_list`: not implemented; the call fails and nothing happens. */
    SYS_SPLICE                   = 0x139, /**< Linux `splice`: not implemented; the call fails and nothing happens. */
    SYS_SYNC_FILE_RANGE          = 0x13A, /**< Linux `sync_file_range`: not implemented; the call fails and nothing happens. */
    SYS_TEE                      = 0x13B, /**< Linux `tee`: not implemented; the call fails and nothing happens. */
    SYS_VMSPLICE                 = 0x13C, /**< Linux `vmsplice`: not implemented; the call fails and nothing happens. */
    SYS_MOVE_PAGES               = 0x13D, /**< Linux `move_pages`: not implemented; the call fails and nothing happens. */
    SYS_GETCPU                   = 0x13E, /**< Linux `getcpu`: not implemented; the call fails and nothing happens. */
    SYS_EPOLL_PWAIT              = 0x13F, /**< Linux `epoll_pwait`: not implemented; the call fails and nothing happens. */
    SYS_UTIMENSAT                = 0x140, /**< Linux `utimensat`: not implemented; the call fails and nothing happens. */
    SYS_SIGNALFD                 = 0x141, /**< Linux `signalfd`: not implemented; the call fails and nothing happens. */
    SYS_TIMERFD_CREATE           = 0x142, /**< Linux `timerfd_create`: not implemented; the call fails and nothing happens. */
    SYS_EVENTFD                  = 0x143, /**< Linux `eventfd`: not implemented; the call fails and nothing happens. */
    SYS_FALLOCATE                = 0x144, /**< Linux `fallocate`: not implemented; the call fails and nothing happens. */
    SYS_TIMERFD_SETTIME          = 0x145, /**< Linux `timerfd_settime`: not implemented; the call fails and nothing happens. */
    SYS_TIMERFD_GETTIME          = 0x146, /**< Linux `timerfd_gettime`: not implemented; the call fails and nothing happens. */
    SYS_SIGNALFD4                = 0x147, /**< Linux `signalfd4`: not implemented; the call fails and nothing happens. */
    SYS_EVENTFD2                 = 0x148, /**< Linux `eventfd2`: not implemented; the call fails and nothing happens. */
    SYS_EPOLL_CREATE1            = 0x149, /**< Linux `epoll_create1`: not implemented; the call fails and nothing happens. */
    SYS_DUP3                     = 0x14A, /**< Linux `dup3`: not implemented; the call fails and nothing happens. */
    SYS_PIPE2                    = 0x14B, /**< Linux `pipe2`: not implemented; the call fails and nothing happens. */
    SYS_INOTIFY_INIT1            = 0x14C, /**< Linux `inotify_init1`: not implemented; the call fails and nothing happens. */
    SYS_PREADV                   = 0x14D, /**< Linux `preadv`: not implemented; the call fails and nothing happens. */
    SYS_PWRITEV                  = 0x14E, /**< Linux `pwritev`: not implemented; the call fails and nothing happens. */
    SYS_RT_TGSIGQUEUEINFO        = 0x14F, /**< Linux `rt_tgsigqueueinfo`: not implemented; the call fails and nothing happens. */
    SYS_PERF_EVENT_OPEN          = 0x150, /**< Linux `perf_event_open`: not implemented; the call fails and nothing happens. */
    SYS_RECVMMSG                 = 0x151, /**< Linux `recvmmsg`: not implemented; the call fails and nothing happens. */
    SYS_FANOTIFY_INIT            = 0x152, /**< Linux `fanotify_init`: not implemented; the call fails and nothing happens. */
    SYS_FANOTIFY_MARK            = 0x153, /**< Linux `fanotify_mark`: not implemented; the call fails and nothing happens. */
    SYS_PRLIMIT64                = 0x154, /**< Linux `prlimit64`: not implemented; the call fails and nothing happens. */
    SYS_NAME_TO_HANDLE_AT        = 0x155, /**< Linux `name_to_handle_at`: not implemented; the call fails and nothing happens. */
    SYS_OPEN_BY_HANDLE_AT        = 0x156, /**< Linux `open_by_handle_at`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_ADJTIME            = 0x157, /**< Linux `clock_adjtime`: not implemented; the call fails and nothing happens. */
    SYS_SYNCFS                   = 0x158, /**< Linux `syncfs`: not implemented; the call fails and nothing happens. */
    SYS_SENDMMSG                 = 0x159, /**< Linux `sendmmsg`: not implemented; the call fails and nothing happens. */
    SYS_SETNS                    = 0x15A, /**< Linux `setns`: not implemented; the call fails and nothing happens. */
    SYS_PROCESS_VM_READV         = 0x15B, /**< Linux `process_vm_readv`: not implemented; the call fails and nothing happens. */
    SYS_PROCESS_VM_WRITEV        = 0x15C, /**< Linux `process_vm_writev`: not implemented; the call fails and nothing happens. */
    SYS_KCMP                     = 0x15D, /**< Linux `kcmp`: not implemented; the call fails and nothing happens. */
    SYS_FINIT_MODULE             = 0x15E, /**< Linux `finit_module`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_SETATTR            = 0x15F, /**< Linux `sched_setattr`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_GETATTR            = 0x160, /**< Linux `sched_getattr`: not implemented; the call fails and nothing happens. */
    SYS_RENAMEAT2                = 0x161, /**< Linux `renameat2`: not implemented; the call fails and nothing happens. */
    SYS_SECCOMP                  = 0x162, /**< Linux `seccomp`: not implemented; the call fails and nothing happens. */
    SYS_GETRANDOM                = 0x163, /**< Linux `getrandom`: not implemented; the call fails and nothing happens. */
    SYS_MEMFD_CREATE             = 0x164, /**< Linux `memfd_create`: not implemented; the call fails and nothing happens. */
    SYS_BPF                      = 0x165, /**< Linux `bpf`: not implemented; the call fails and nothing happens. */
    SYS_EXECVEAT                 = 0x166, /**< Linux `execveat`: not implemented; the call fails and nothing happens. */
    SYS_SOCKET                   = 0x167, /**< Linux `socket`: not implemented; the call fails and nothing happens. */
    SYS_SOCKETPAIR               = 0x168, /**< Linux `socketpair`: not implemented; the call fails and nothing happens. */
    SYS_BIND                     = 0x169, /**< Linux `bind`: not implemented; the call fails and nothing happens. */
    SYS_CONNECT                  = 0x16A, /**< Linux `connect`: not implemented; the call fails and nothing happens. */
    SYS_LISTEN                   = 0x16B, /**< Linux `listen`: not implemented; the call fails and nothing happens. */
    SYS_ACCEPT4                  = 0x16C, /**< Linux `accept4`: not implemented; the call fails and nothing happens. */
    SYS_GETSOCKOPT               = 0x16D, /**< Linux `getsockopt`: not implemented; the call fails and nothing happens. */
    SYS_SETSOCKOPT               = 0x16E, /**< Linux `setsockopt`: not implemented; the call fails and nothing happens. */
    SYS_GETSOCKNAME              = 0x16F, /**< Linux `getsockname`: not implemented; the call fails and nothing happens. */
    SYS_GETPEERNAME              = 0x170, /**< Linux `getpeername`: not implemented; the call fails and nothing happens. */
    SYS_SENDTO                   = 0x171, /**< Linux `sendto`: not implemented; the call fails and nothing happens. */
    SYS_SENDMSG                  = 0x172, /**< Linux `sendmsg`: not implemented; the call fails and nothing happens. */
    SYS_RECVFROM                 = 0x173, /**< Linux `recvfrom`: not implemented; the call fails and nothing happens. */
    SYS_RECVMSG                  = 0x174, /**< Linux `recvmsg`: not implemented; the call fails and nothing happens. */
    SYS_SHUTDOWN                 = 0x175, /**< Linux `shutdown`: not implemented; the call fails and nothing happens. */
    SYS_USERFAULTFD              = 0x176, /**< Linux `userfaultfd`: not implemented; the call fails and nothing happens. */
    SYS_MEMBARRIER               = 0x177, /**< Linux `membarrier`: not implemented; the call fails and nothing happens. */
    SYS_MLOCK2                   = 0x178, /**< Linux `mlock2`: not implemented; the call fails and nothing happens. */
    SYS_COPY_FILE_RANGE          = 0x179, /**< Linux `copy_file_range`: not implemented; the call fails and nothing happens. */
    SYS_PREADV2                  = 0x17A, /**< Linux `preadv2`: not implemented; the call fails and nothing happens. */
    SYS_PWRITEV2                 = 0x17B, /**< Linux `pwritev2`: not implemented; the call fails and nothing happens. */
    SYS_PKEY_MPROTECT            = 0x17C, /**< Linux `pkey_mprotect`: not implemented; the call fails and nothing happens. */
    SYS_PKEY_ALLOC               = 0x17D, /**< Linux `pkey_alloc`: not implemented; the call fails and nothing happens. */
    SYS_PKEY_FREE                = 0x17E, /**< Linux `pkey_free`: not implemented; the call fails and nothing happens. */
    SYS_STATX                    = 0x17F, /**< Linux `statx`: not implemented; the call fails and nothing happens. */
    SYS_ARCH_PRCTL               = 0x180, /**< Linux `arch_prctl`: not implemented; the call fails and nothing happens. */
    SYS_IO_PGETEVENTS            = 0x181, /**< Linux `io_pgetevents`: not implemented; the call fails and nothing happens. */
    SYS_RSEQ                     = 0x182, /**< Linux `rseq`: not implemented; the call fails and nothing happens. */
    SYS_SEMGET                   = 0x189, /**< Linux `semget`: not implemented; the call fails and nothing happens. */
    SYS_SEMCTL                   = 0x18A, /**< Linux `semctl`: not implemented; the call fails and nothing happens. */
    SYS_SHMGET                   = 0x18B, /**< Linux `shmget`: not implemented; the call fails and nothing happens. */
    SYS_SHMCTL                   = 0x18C, /**< Linux `shmctl`: not implemented; the call fails and nothing happens. */
    SYS_SHMAT                    = 0x18D, /**< Linux `shmat`: not implemented; the call fails and nothing happens. */
    SYS_SHMDT                    = 0x18E, /**< Linux `shmdt`: not implemented; the call fails and nothing happens. */
    SYS_MSGGET                   = 0x18F, /**< Linux `msgget`: not implemented; the call fails and nothing happens. */
    SYS_MSGSND                   = 0x190, /**< Linux `msgsnd`: not implemented; the call fails and nothing happens. */
    SYS_MSGRCV                   = 0x191, /**< Linux `msgrcv`: not implemented; the call fails and nothing happens. */
    SYS_MSGCTL                   = 0x192, /**< Linux `msgctl`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_GETTIME64          = 0x193, /**< Linux `clock_gettime64`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_SETTIME64          = 0x194, /**< Linux `clock_settime64`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_ADJTIME64          = 0x195, /**< Linux `clock_adjtime64`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_GETRES_TIME64      = 0x196, /**< Linux `clock_getres_time64`: not implemented; the call fails and nothing happens. */
    SYS_CLOCK_NANOSLEEP_TIME64   = 0x197, /**< Linux `clock_nanosleep_time64`: not implemented; the call fails and nothing happens. */
    SYS_TIMER_GETTIME64          = 0x198, /**< Linux `timer_gettime64`: not implemented; the call fails and nothing happens. */
    SYS_TIMER_SETTIME64          = 0x199, /**< Linux `timer_settime64`: not implemented; the call fails and nothing happens. */
    SYS_TIMERFD_GETTIME64        = 0x19A, /**< Linux `timerfd_gettime64`: not implemented; the call fails and nothing happens. */
    SYS_TIMERFD_SETTIME64        = 0x19B, /**< Linux `timerfd_settime64`: not implemented; the call fails and nothing happens. */
    SYS_UTIMENSAT_TIME64         = 0x19C, /**< Linux `utimensat_time64`: not implemented; the call fails and nothing happens. */
    SYS_PSELECT6_TIME64          = 0x19D, /**< Linux `pselect6_time64`: not implemented; the call fails and nothing happens. */
    SYS_PPOLL_TIME64             = 0x19E, /**< Linux `ppoll_time64`: not implemented; the call fails and nothing happens. */
    SYS_IO_PGETEVENTS_TIME64     = 0x1A0, /**< Linux `io_pgetevents_time64`: not implemented; the call fails and nothing happens. */
    SYS_RECVMMSG_TIME64          = 0x1A1, /**< Linux `recvmmsg_time64`: not implemented; the call fails and nothing happens. */
    SYS_MQ_TIMEDSEND_TIME64      = 0x1A2, /**< Linux `mq_timedsend_time64`: not implemented; the call fails and nothing happens. */
    SYS_MQ_TIMEDRECEIVE_TIME64   = 0x1A3, /**< Linux `mq_timedreceive_time64`: not implemented; the call fails and nothing happens. */
    SYS_SEMTIMEDOP_TIME64        = 0x1A4, /**< Linux `semtimedop_time64`: not implemented; the call fails and nothing happens. */
    SYS_RT_SIGTIMEDWAIT_TIME64   = 0x1A5, /**< Linux `rt_sigtimedwait_time64`: not implemented; the call fails and nothing happens. */
    SYS_FUTEX_TIME64             = 0x1A6, /**< Linux `futex_time64`: not implemented; the call fails and nothing happens. */
    SYS_SCHED_RR_GET_INTERVAL_TIME64 = 0x1A7, /**< Linux `sched_rr_get_interval_time64`: not implemented; the call fails and nothing happens. */
    SYS_PIDFD_SEND_SIGNAL        = 0x1A8, /**< Linux `pidfd_send_signal`: not implemented; the call fails and nothing happens. */
    SYS_IO_URING_SETUP           = 0x1A9, /**< Linux `io_uring_setup`: not implemented; the call fails and nothing happens. */
    SYS_IO_URING_ENTER           = 0x1AA, /**< Linux `io_uring_enter`: not implemented; the call fails and nothing happens. */
    SYS_IO_URING_REGISTER        = 0x1AB, /**< Linux `io_uring_register`: not implemented; the call fails and nothing happens. */
    SYS_OPEN_TREE                = 0x1AC, /**< Linux `open_tree`: not implemented; the call fails and nothing happens. */
    SYS_MOVE_MOUNT               = 0x1AD, /**< Linux `move_mount`: not implemented; the call fails and nothing happens. */
    SYS_FSOPEN                   = 0x1AE, /**< Linux `fsopen`: not implemented; the call fails and nothing happens. */
    SYS_FSCONFIG                 = 0x1AF, /**< Linux `fsconfig`: not implemented; the call fails and nothing happens. */
    SYS_FSMOUNT                  = 0x1B0, /**< Linux `fsmount`: not implemented; the call fails and nothing happens. */
    SYS_FSPICK                   = 0x1B1, /**< Linux `fspick`: not implemented; the call fails and nothing happens. */
    SYS_PIDFD_OPEN               = 0x1B2, /**< Linux `pidfd_open`: not implemented; the call fails and nothing happens. */
    SYS_CLONE3                   = 0x1B3, /**< Linux `clone3`: not implemented; the call fails and nothing happens. */
    SYS_CLOSE_RANGE              = 0x1B4, /**< Linux `close_range`: not implemented; the call fails and nothing happens. */
    SYS_FACCESSAT2               = 0x1B7, /**< Linux `faccessat2`: not implemented; the call fails and nothing happens. */

    SYS_PROC_CREATE              = 0x200, /**< Create a process with an empty address space and no thread, and return a handle for it (needs a SPAWN capability); the fourth argument optionally points to the service label and IPC labels the process is born with. */
    SYS_PROC_MAP                 = 0x201, /**< Map fresh zeroed writable pages into a process that was created and not started. */
    SYS_PROC_WRITE               = 0x202, /**< Copy bytes from the caller into the mapped pages of a process that was created and not started. */
    SYS_PROC_PROTECT             = 0x203, /**< Make mapped pages of a process that was created and not started read only. */
    SYS_PROC_START               = 0x204, /**< Create the first thread of a process that was created, at an entry point, and let it run; returns the process id. */
    SYS_PROC_DISCARD             = 0x205, /**< Destroy a process that was created and not started. */
    SYS_PROC_ALIVE               = 0x206, /**< Tell whether the thread with an id (a process id names its first thread) is alive. */
    SYS_THREAD_CREATE            = 0x207, /**< Create a thread in the calling process. */
    SYS_THREAD_PROCESS           = 0x208, /**< Give the process a thread belongs to, so a server that sees the id of the calling thread knows whose it is. */
    SYS_PROC_EXEC                = 0x20A, /**< Replace the program image of a process that waits for the reply of a call the caller is serving with the pending process the caller built, keeping its id, capabilities and endpoints; the other threads of the process end (needs a SPAWN capability). */
    SYS_PROC_FORK                = 0x20B, /**< Make a copy of a process that waits for the reply of a call the caller is serving: a new child of the caller with the address space shared copy on write, the inheritable capabilities, and one thread that resumes inside the same call with the answer the caller supplies (needs a SPAWN capability). */
    SYS_PROC_STATE               = 0x209,/**< Tell in which states the live threads of a process are, as a mask of bits; the process manager turns it into the state `R` or `S`. */

    SYS_SLEEP                    = 0x210, /**< Sleep for a number of milliseconds (0 only yields). */
    SYS_FUTEX_WAIT               = 0x211, /**< Block on a user address while it holds an expected value. */
    SYS_FUTEX_WAKE               = 0x212, /**< Wake threads blocked on a user address. */
    SYS_SCHED_METRICS            = 0x213, /**< Write the response, CPU and turnaround ticks of the calling thread; registered only in a kernel built with SCHED_METRICS=1. */

    SYS_IPC_SEND                 = 0x220, /**< Send an IPC message to the inbox of a thread (a process id reaches its first thread). */
    SYS_IPC_RECV                 = 0x221, /**< Block until an interrupt notification or a message in the inbox of the calling thread arrives. */
    SYS_IPC_TRY_RECV             = 0x222, /**< Non blocking IPC receive: 0 message, 1 nothing pending. */
    SYS_IPC_RECV_TIMEOUT         = 0x223, /**< IPC receive that gives up after a timeout in milliseconds: 0 message, 1 timeout. */
    SYS_EP_SEND                  = 0x224, /**< Send an IPC message through a handle on an endpoint; the kernel stamps the badge of the handle in the message, and opens a reply token when asked. */
    SYS_EP_REPLY                 = 0x225, /**< Answer a message that carried a reply token: the answer goes to the thread that asked, and the token is spent. */
    SYS_EP_FORWARD               = 0x226, /**< Pass a message that carried a reply token to another endpoint, with the token, so the new receiver answers the thread that asked. */

    SYS_IPC_CALL                 = 0x230, /**< Synchronous IPC: send four words to a server thread and wait for the reply, with an optional time limit in milliseconds. */
    SYS_IPC_ACCEPT               = 0x231, /**< Server side: wait for the next synchronous request. */
    SYS_IPC_REPLY                = 0x232, /**< Server side: answer the request being served. */
    SYS_IPC_REPLY_WAIT           = 0x233, /**< Server side: answer and wait for the next request in one call. */
    SYS_IPC_DEFER                = 0x234, /**< Server side: postpone the reply of the current request. */
    SYS_IPC_REPLY_TO             = 0x235, /**< Server side: answer a request that was deferred, named by the ticket that SYS_IPC_DEFER returned. */
    SYS_EP_CALL                  = 0x236, /**< Synchronous IPC through a handle on an endpoint; the server reads the badge of the handle as the peer of its accept. */

    SYS_CAP_GRANT                = 0x240, /**< Grant part of a grantable capability held by the caller to another process; returns the id of the derivation tree node, or -1. */
    SYS_CAP_REVOKE               = 0x241, /**< Revoke a node of the derivation tree and every capability derived from it; the caller must hold an ancestor of the node. */
    SYS_CAP_ACCEPT               = 0x242, /**< Choose which capability types a CAPABILITY_TRANSFER message may give the calling process; none until the call is made. */
    SYS_SERVICE_SET              = 0x243, /**< Give a process its service label, once; the caller must hold grant authority. */
    SYS_EP_CREATE                = 0x244, /**< Create an endpoint and get the capability that owns it (grant authority only). */
    SYS_EP_BIND                  = 0x245, /**< Bind an endpoint to the process that receives from it; needs the bind right of the endpoint. */

    SYS_REQUEST_RESOURCE         = 0x250, /**< Ask for I/O ports (first port and count); allowed only if the caller holds a covering IO_PORTS capability and the range avoids the PIC and timer ports. */
    SYS_IRQ_BIND                 = 0x251, /**< Bind a hardware interrupt line to the calling thread (needs an IRQ capability). */
    SYS_IRQ_ACK                  = 0x252, /**< Acknowledge a bound interrupt and re-enable the line. */

    SYS_SHM_CREATE               = 0x260, /**< Create a shared memory object. */
    SYS_SHM_SHARE                = 0x261, /**< Share a shared memory object with another process. */
    SYS_SHM_MAP                  = 0x262, /**< Map a shared memory object into the caller. */
    SYS_SHM_RELEASE              = 0x263, /**< Drop the caller's reference to a shared memory object. */

    SYS_MODULE_PROCESS           = 0x270, /**< Find the process the kernel started from a boot module, by module name (grant authority only). */
    SYS_MODULE_OPEN              = 0x271, /**< Open a boot data module by name as a read only shared object (needs a BOOT_MODULE capability). */
    SYS_MODULE_INDEX             = 0x272, /**< Resolve a boot data module name to its index (grant authority only). */
};

/** @brief Number of entries of the table of the dispatcher: one past the highest system call number. Raise it when a call is added above the last one; the `static_assert` below fails if it is not. */
constexpr uint32_t SYSCALL_TABLE_SIZE = 0x273;

static_assert(SYS_MODULE_INDEX < SYSCALL_TABLE_SIZE, "the table must hold every system call number");

}

#endif // !X86_COCK_SYSCALL_NUMS_HPP
