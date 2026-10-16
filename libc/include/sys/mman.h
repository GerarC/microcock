/**
 * @file mman.h
 * @brief Memory mapping on top of the kernel mmap syscall.
 *
 * Deviation from POSIX: files cannot be mapped (`fd` is ignored); the calls
 * map anonymous pages (and physical ranges with `MAP_PHYS` for drivers that
 * hold the capability). The kernel validates every argument.
 */
#ifndef SYS_MMAN_H
#define SYS_MMAN_H

#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

/** Pages cannot be accessed. */
#define PROT_NONE  0x00
/** Pages can be read. */
#define PROT_READ  0x01
/** Pages can be written. */
#define PROT_WRITE 0x02
/** Pages can be executed. */
#define PROT_EXEC  0x04

/** Share the mapping (shared memory objects). */
#define MAP_SHARED    0x01
/** Private mapping. */
#define MAP_PRIVATE   0x02
/** Use exactly the requested address. */
#define MAP_FIXED     0x10
/** Anonymous zeroed memory, no backing file. */
#define MAP_ANONYMOUS 0x20
/** MicroCOCK extension: map a physical range (MMIO or DMA), privileged. */
#define MAP_PHYS      0x1000

/** Value returned by `mmap` on failure. */
#define MAP_FAILED ((void *)-1)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maps pages into the address space of the caller.
 * @param addr Requested address (hint, or exact with `MAP_FIXED`); may be null.
 * @param length Size in bytes, rounded up to whole 4 KiB pages.
 * @param prot Combination of `PROT_*`.
 * @param flags Combination of `MAP_*`.
 * @param fd Ignored for anonymous mappings; pass -1.
 * @param offset Ignored for anonymous mappings; with `MAP_PHYS` it is the
 *               physical address.
 * @return Address of the mapping, or `MAP_FAILED`. Note: this implementation
 *         does not set `errno`.
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, size_t offset);

/**
 * @brief Removes a mapping.
 * @param addr Page aligned start of a mapping owned by the caller.
 * @param length Size in bytes.
 * @return 0 on success, -1 on failure (`errno` is not set).
 */
int munmap(void *addr, size_t length);

#ifdef __cplusplus
}
#endif

#endif // SYS_MMAN_H
