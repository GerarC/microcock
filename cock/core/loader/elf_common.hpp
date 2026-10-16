/** @file
 * @brief Constants of the ELF format shared by the 32 and 64 bit classes.
 */
#ifndef CORE_LOADER_ELF_COMMON_HPP
#define CORE_LOADER_ELF_COMMON_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::loader {

/** @brief Size in bytes of the e_ident array at the start of every ELF file. */
constexpr size_t ELF_IDENT_SIZE = 0x10;
/** @brief Index in e_ident of the class byte (32 or 64 bit). */
constexpr size_t ELF_IDENT_CLASS = 4;
/** @brief Index in e_ident of the data encoding byte (endianness). */
constexpr size_t ELF_IDENT_DATA = 5;
/** @brief Index in e_ident where the magic bytes start. */
constexpr size_t ELF_IDENT_MAGIC = 0;
/** @brief Index in e_ident of the ELF version byte. */
constexpr size_t ELF_IDENT_VERSION = 6;
/** @brief Number of magic bytes at the start of the file. */
constexpr size_t ELF_MAGIC_SIZE = 4;
/** @brief The magic bytes: 0x7F followed by the letters E, L and F. */
constexpr uint8_t ELF_MAGIC[ELF_MAGIC_SIZE] = {0x7F, 'E', 'L', 'F'};
/** @brief The only ELF version defined by the specification. */
constexpr uint8_t ELF_VERSION_CURRENT = 1;
/** @brief Data encoding value for little endian files. */
constexpr uint8_t ELF_DATA_LSB = 1;
/** @brief Data encoding value for big endian files. */
constexpr uint8_t ELF_DATA_MSB = 2;
/** @brief File type of a fixed address executable, the only type loaded. */
constexpr uint16_t ELF_TYPE_EXEC = 2;
/** @brief File type of a shared object or position independent executable. */
constexpr uint16_t ELF_TYPE_DYN = 3;
/** @brief Program header type of a loadable segment. */
constexpr uint32_t ELF_PT_LOAD = 1;
/** @brief Program header flag: the segment is writable. */
constexpr uint32_t ELF_PF_WRITE = 2;

} // namespace cock::core::loader

#endif // !CORE_LOADER_ELF_COMMON_HPP
