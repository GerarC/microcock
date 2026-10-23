/** @file
 * @brief Template implementation of the ELF loader, instantiated once per ELF
 * class (Elf32, Elf64) from elf.cpp. Internal to the loader.
 */
#ifndef CORE_LOADER_ELF_LOADER_HPP
#define CORE_LOADER_ELF_LOADER_HPP

#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/loader/elf.hpp>
#include <cock/core/loader/elf_common.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <hal/executable.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::core::loader::detail {

static_assert((hal::ELF_DATA_ENCODING == ELF_DATA_LSB) ==
				  (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__),
			  "ELF data encoding of the architecture does not match the "
			  "byte order of the compiler");

/**
 * @brief Checks the file header of an untrusted image.
 * @tparam Format Class traits, Elf32 or Elf64.
 * @param header Copy of the file header.
 * @param size Size of the whole image in bytes.
 * @return True when class, data encoding, type (executable only), machine and
 * program header entry size match, there is at least one program header and
 * the whole table lies inside the image. No allocation, no side effects.
 */
template <typename Format>
bool validate_header(const typename Format::Header &header, size_t size) {
	using Segment = typename Format::ProgramHeader;

	if (header.ident[ELF_IDENT_CLASS] != Format::CLASS_ID) return false;
	if (header.ident[ELF_IDENT_DATA] != hal::ELF_DATA_ENCODING) return false;
	if (header.type != ELF_TYPE_EXEC) return false;
	if (header.machine != Format::MACHINE) return false;
	if (header.program_header_entry_size != sizeof(Segment)) return false;
	if (header.program_header_num == 0) return false;
	if (header.program_header_offset > size) return false;

	uint64_t available = size - header.program_header_offset;
	return header.program_header_num <= available / sizeof(Segment);
}

/**
 * @brief Checks one loadable segment against the image and the user range.
 * @tparam Format Class traits, Elf32 or Elf64.
 * @param segment Copy of the program header.
 * @param size Size of the whole image in bytes.
 * @param image_limit First address a segment may not reach, the user space
 * limit minus the user stack.
 * @return True when file size does not exceed memory size, the file contents
 * lie inside the image, the segment starts above the first page and ends
 * below the limit, all without integer overflow. An empty segment is valid.
 */
template <typename Format>
bool validate_segment(const typename Format::ProgramHeader &segment,
					  size_t size, uint64_t image_limit) {
	if (segment.size_in_file > segment.size_in_memory) return false;
	if (segment.size_in_memory == 0) return true;
	if (segment.offset > size) return false;
	if (segment.size_in_file > size - segment.offset) return false;
	if (segment.vaddr < hal::PAGE_SIZE) return false;
	if (segment.size_in_memory > image_limit) return false;
	return segment.vaddr <= image_limit - segment.size_in_memory;
}

/**
 * @brief Tells whether an address falls inside a segment in memory.
 * @tparam Format Class traits, Elf32 or Elf64.
 * @param segment Program header to test.
 * @param entry Virtual address, normally the entry point.
 * @return True when the address is within the segment memory size.
 */
template <typename Format>
bool contains_entry(const typename Format::ProgramHeader &segment,
					typename Format::Address entry) {
	return entry >= segment.vaddr &&
		   entry - segment.vaddr < segment.size_in_memory;
}

/**
 * @brief Copies one program header out of the image.
 * @tparam Format Class traits, Elf32 or Elf64.
 * @param base Start of the image.
 * @param header Validated file header.
 * @param index Program header index, below program_header_num.
 * @return A copy of the program header, so alignment of the image never
 * matters.
 * @pre validate_header succeeded, which guarantees the table is in bounds.
 */
template <typename Format>
typename Format::ProgramHeader read_segment(const uint8_t *base,
											const typename Format::Header &header,
											size_t index) {
	typename Format::ProgramHeader segment;
	uint64_t offset = header.program_header_offset +
					  static_cast<uint64_t>(index) *
						  sizeof(typename Format::ProgramHeader);
	memcpy(&segment, base + offset, sizeof(segment));
	return segment;
}

/**
 * @brief Tells whether any writable loadable segment touches a page.
 * @tparam Format Class traits, Elf32 or Elf64.
 * @param base Start of the image.
 * @param header Validated file header.
 * @param page_start Page aligned virtual address of the page.
 * @return True when a writable segment overlaps the page, in which case the
 * page must stay writable even if a read only segment shares it.
 */
template <typename Format>
bool writable_segment_covers_page(const uint8_t *base,
								  const typename Format::Header &header,
								  uint64_t page_start) {
	uint64_t page_end = page_start + hal::PAGE_SIZE;

	for (size_t idx = 0; idx < header.program_header_num; idx++) {
		typename Format::ProgramHeader segment =
			read_segment<Format>(base, header, idx);
		if (segment.type != ELF_PT_LOAD || !(segment.flags & ELF_PF_WRITE) ||
			segment.size_in_memory == 0)
			continue;

		uint64_t segment_start = segment.vaddr & ~static_cast<uint64_t>(hal::PAGE_MASK);
		uint64_t segment_end = segment.vaddr + segment.size_in_memory;
		if (segment_start < page_end && segment_end > page_start) return true;
	}
	return false;
}

/**
 * @brief Removes write permission from the pages of read only segments.
 * @tparam Format Class traits, Elf32 or Elf64.
 * @param base Start of the image.
 * @param header Validated file header.
 * @pre The target address space is the active one and every segment is
 * already mapped and filled.
 * @note Pages shared with a writable segment are left writable.
 */
template <typename Format>
void seal_read_only_segments(const uint8_t *base,
							 const typename Format::Header &header) {
	using memory::VirtAddr;
	using memory::VirtualMemoryManager;
	using memory::VMMPermission;

	constexpr uint64_t page_mask = hal::PAGE_MASK;

	for (size_t idx = 0; idx < header.program_header_num; idx++) {
		typename Format::ProgramHeader segment =
			read_segment<Format>(base, header, idx);
		if (segment.type != ELF_PT_LOAD || segment.size_in_memory == 0 ||
			(segment.flags & ELF_PF_WRITE))
			continue;

		uint64_t first_page = segment.vaddr & ~page_mask;
		uint64_t page_count =
			((segment.vaddr & page_mask) + segment.size_in_memory + page_mask) /
			hal::PAGE_SIZE;

		for (uint64_t page = 0; page < page_count; page++) {
			uint64_t page_start = first_page + page * hal::PAGE_SIZE;
			if (writable_segment_covers_page<Format>(base, header, page_start))
				continue;
			VirtualMemoryManager::protectPage(
				VirtAddr(static_cast<uintptr_t>(page_start)),
				VMMPermission::USER_CODE);
		}
	}
}

/**
 * @brief Validates an image of one ELF class and loads it into an address
 * space.
 *
 * Validates the header, every loadable segment and the entry point first, then
 * with interrupts blocked switches to the target address space, maps each
 * segment, copies its file contents (the rest of the pages is zero), seals the
 * read only ones and switches back.
 *
 * @tparam Format Class traits, Elf32 or Elf64.
 * @param base Start of the image. Untrusted.
 * @param size Size of the image in bytes.
 * @param address_space Physical root of the destination address space.
 * @param out Receives the entry point and user space limit on success.
 * @return True on success. False on any validation error or when a segment
 * cannot be mapped, in which case earlier segments stay mapped.
 */
template <typename Format>
bool load_image(const uint8_t *base, size_t size, memory::PhysAddr address_space,
				LoadedImage &out) {
	using Header = typename Format::Header;
	using Segment = typename Format::ProgramHeader;
	using memory::PhysAddr;
	using memory::VirtAddr;
	using memory::VirtualMemoryManager;
	using memory::VMMPermission;
	using utils::Logger;

	constexpr uint64_t user_limit = Format::USER_SPACE_LIMIT;
	static_assert(user_limit > task::USER_STACK_SIZE + hal::PAGE_SIZE,
				  "user space of this ELF class is too small");
	static_assert(user_limit <= hal::USER_SPACE_LIMIT,
				  "ELF class limit exceeds the architecture user space");
	constexpr uint64_t image_limit = user_limit - task::USER_STACK_SIZE;
	constexpr uint64_t page_mask = hal::PAGE_MASK;

	if (size < sizeof(Header)) {
		Logger::error("ELF: file is smaller than its header");
		return false;
	}

	Header header;
	memcpy(&header, base, sizeof(header));
	if (!validate_header<Format>(header, size)) {
		Logger::error("ELF: invalid or unsupported header");
		return false;
	}

	bool entry_found = false;
	for (size_t idx = 0; idx < header.program_header_num; idx++) {
		Segment segment = read_segment<Format>(base, header, idx);
		if (segment.type != ELF_PT_LOAD) continue;
		if (!validate_segment<Format>(segment, size, image_limit)) {
			Logger::error("ELF: segment %u is out of bounds",
						  static_cast<unsigned>(idx));
			return false;
		}
		if (contains_entry<Format>(segment, header.entry)) entry_found = true;
	}
	if (!entry_found) {
		Logger::error("ELF: entry point is not inside a loaded segment");
		return false;
	}

	hal::InterruptGuard guard;
	PhysAddr old_cr3 = hal::get_current_address_space();
	hal::switch_address_space(address_space);

	bool loaded = true;
	for (size_t idx = 0; idx < header.program_header_num && loaded; idx++) {
		Segment segment = read_segment<Format>(base, header, idx);
		if (segment.type != ELF_PT_LOAD || segment.size_in_memory == 0)
			continue;

		uint64_t first_page = segment.vaddr & ~page_mask;
		uint64_t page_count =
			((segment.vaddr & page_mask) + segment.size_in_memory + page_mask) /
			hal::PAGE_SIZE;

		if (VirtualMemoryManager::mapPagesAt(
					VirtAddr(static_cast<uintptr_t>(first_page)),
					static_cast<size_t>(page_count), VMMPermission::USER_DATA,
					VirtAddr(static_cast<uintptr_t>(user_limit)))
				.isNull()) {
			Logger::error("ELF: cannot map segment %u",
						  static_cast<unsigned>(idx));
			loaded = false;
			continue;
		}

		memcpy(reinterpret_cast<void *>(static_cast<uintptr_t>(segment.vaddr)),
			   base + segment.offset, static_cast<size_t>(segment.size_in_file));
	}

	if (loaded) seal_read_only_segments<Format>(base, header);

	hal::switch_address_space(old_cr3);
	if (!loaded) return false;

	out.entry = VirtAddr(static_cast<uintptr_t>(header.entry));
	out.user_space_limit = VirtAddr(static_cast<uintptr_t>(user_limit));
	return true;
}

} // namespace cock::core::loader::detail

#endif // !CORE_LOADER_ELF_LOADER_HPP
