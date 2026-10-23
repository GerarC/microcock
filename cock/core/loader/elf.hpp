/** @file
 * @brief Public interface of the ELF loader: detection and loading of
 * executables into a user address space.
 *
 * Only `cock_main` uses it, for the Multiboot boot modules, which start before
 * any program could load them. No system call reaches it: a program started at
 * run time is loaded in user space (`cock::user::spawn`) through the
 * `SYS_PROC_*` calls.
 */
#ifndef CORE_LOADER_ELF_HPP
#define CORE_LOADER_ELF_HPP

#include <cock/core/memory/address.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::loader {

/**
 * @brief Result of loading an executable.
 */
struct LoadedImage {
	memory::VirtAddr entry; /**< Address where the first thread starts. */
	memory::VirtAddr user_space_limit; /**< Upper bound of the user address
										  space for the ELF class that was
										  loaded. The user stack lives just
										  below it. */
};

/**
 * @brief Tells whether a buffer starts with the ELF magic bytes.
 * @param image Start of the buffer.
 * @param size Size of the buffer in bytes.
 * @return True when the buffer is large enough and the magic matches.
 */
bool has_elf_magic(const void *image, size_t size);

/**
 * @brief Validates an ELF executable and maps its loadable segments.
 * @param image Start of the executable in kernel memory. Untrusted contents.
 * @param size Size of the image in bytes.
 * @param address_space Physical root of the address space that receives the
 * segments.
 * @param out Filled with the entry point and the limit on success.
 * @return True on success. False on a null image or root, a bad magic, an
 * unknown or unsupported ELF class, a header that is not a valid executable of
 * this architecture, a segment outside the image or the user range, an entry
 * point outside every loadable segment, or when memory runs out. @p out is
 * zeroed first.
 * @note Interrupts are blocked while the target address space is active, and
 * the previous one is restored before returning. Read only segments are
 * sealed after copying. A failure while mapping may leave partial mappings in
 * the address space, the caller destroys it.
 */
bool load_executable(const void *image, size_t size,
					 memory::PhysAddr address_space, LoadedImage &out);

} // namespace cock::core::loader

#endif // !CORE_LOADER_ELF_HPP
