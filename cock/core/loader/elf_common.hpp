#ifndef CORE_LOADER_ELF_COMMON_HPP
#define CORE_LOADER_ELF_COMMON_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::loader {

constexpr size_t ELF_IDENT_SIZE = 0x10;
constexpr size_t ELF_IDENT_CLASS = 4;
constexpr size_t ELF_IDENT_DATA = 5;
constexpr size_t ELF_IDENT_MAGIC = 0;
constexpr size_t ELF_IDENT_VERSION = 6;
constexpr size_t ELF_MAGIC_SIZE = 4;
constexpr uint8_t ELF_MAGIC[ELF_MAGIC_SIZE] = {0x7F, 'E', 'L', 'F'};
constexpr uint8_t ELF_VERSION_CURRENT = 1;
constexpr uint8_t ELF_DATA_LSB = 1;
constexpr uint8_t ELF_DATA_MSB = 2;
constexpr uint16_t ELF_TYPE_EXEC = 2;
constexpr uint16_t ELF_TYPE_DYN = 3;
constexpr uint32_t ELF_PT_LOAD = 1;
constexpr uint32_t ELF_PF_WRITE = 2;

} // namespace cock::core::loader

#endif // !CORE_LOADER_ELF_COMMON_HPP
