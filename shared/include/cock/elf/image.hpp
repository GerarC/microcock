/**
 * @file image.hpp
 * @brief Reading a 32 bit ELF executable that nobody trusts, for the loader in user space.
 *
 * The parser only reads: it never maps or writes memory, so a mistake in it can make a program
 * fail to start but can never reach the kernel, which validates every range again. It has no
 * dependencies, so the host tests run it on files they build by hand.
 */
#ifndef SHARED_COCK_ELF_IMAGE_HPP
#define SHARED_COCK_ELF_IMAGE_HPP

#include <cock/abi/executable.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace cock::elf {

/** Size in bytes of `e_ident`. */
constexpr size_t IDENT_SIZE = 16;
/** Index in `e_ident` of the class byte. */
constexpr size_t IDENT_CLASS = 4;
/** Index in `e_ident` of the data encoding byte. */
constexpr size_t IDENT_DATA = 5;
/** Index in `e_ident` of the version byte. */
constexpr size_t IDENT_VERSION = 6;
/** Number of magic bytes at the start of the file. */
constexpr size_t MAGIC_SIZE = 4;
/** The magic bytes: 0x7F and the letters E, L and F. */
constexpr uint8_t MAGIC[MAGIC_SIZE] = {0x7F, 'E', 'L', 'F'};
/** The only version of the format. */
constexpr uint8_t VERSION_CURRENT = 1;
/** `e_type` of an executable at fixed addresses, the only type accepted. */
constexpr uint16_t TYPE_EXECUTABLE = 2;
/** `p_type` of a segment that is loaded. */
constexpr uint32_t SEGMENT_LOAD = 1;
/** `p_flags` bit of a writable segment. */
constexpr uint32_t SEGMENT_WRITABLE = 2;
/** Size of the file header of the 32 bit class. */
constexpr size_t HEADER_SIZE = 52;
/** Size of one program header of the 32 bit class. */
constexpr size_t PROGRAM_HEADER_SIZE = 32;
/** Most loadable segments of one image; more are refused. */
constexpr size_t MAX_SEGMENTS = 8;
/** Size of a page of the target, as the kernel maps memory. */
constexpr uint32_t PAGE_BYTES = 0x1000;
/** Mask of the offset inside a page. */
constexpr uint32_t PAGE_MASK = PAGE_BYTES - 1;
/** Bytes at the start of the address space that no segment may use. */
constexpr uint32_t RESERVED_LOW_BYTES = PAGE_BYTES;

/** File header of the 32 bit class, copied before use. */
struct Header {
	uint8_t ident[IDENT_SIZE]; /**< Magic, class, encoding, version. */
	uint16_t type; /**< Kind of file. */
	uint16_t machine; /**< Target architecture. */
	uint32_t version; /**< Version of the file. */
	uint32_t entry; /**< Address where the program starts. */
	uint32_t programHeaderOffset; /**< Offset of the program header table. */
	uint32_t sectionHeaderOffset; /**< Ignored. */
	uint32_t flags; /**< Ignored. */
	uint16_t headerSize; /**< Ignored. */
	uint16_t programHeaderSize; /**< Size of one program header. */
	uint16_t programHeaderCount; /**< Number of program headers. */
	uint16_t sectionHeaderSize; /**< Ignored. */
	uint16_t sectionHeaderCount; /**< Ignored. */
	uint16_t sectionNameIndex; /**< Ignored. */
};

/** Program header of the 32 bit class, copied before use. */
struct ProgramHeader {
	uint32_t type; /**< Kind of segment. */
	uint32_t offset; /**< Offset of the contents in the file. */
	uint32_t address; /**< Address in the program. */
	uint32_t physical; /**< Ignored. */
	uint32_t fileSize; /**< Bytes taken from the file. */
	uint32_t memorySize; /**< Bytes in memory, the rest is zero. */
	uint32_t flags; /**< Permissions. */
	uint32_t alignment; /**< Ignored. */
};

static_assert(sizeof(Header) == HEADER_SIZE, "the ELF32 file header has the size of the specification");
static_assert(sizeof(ProgramHeader) == PROGRAM_HEADER_SIZE,
			  "the ELF32 program header has the size of the specification");

/** One segment to map and fill. */
struct Segment {
	uint32_t address; /**< Start in the program, not page aligned necessarily. */
	uint32_t offset; /**< Where the contents start in the file. */
	uint32_t fileSize; /**< Bytes to copy. */
	uint32_t memorySize; /**< Bytes to map, at least `fileSize`, non zero. */
	bool writable; /**< False when the pages are sealed after the copy. */
};

/** What the loader needs from an image: where to start and what to map. */
struct Image {
	uint32_t entry; /**< Address of the first instruction. */
	size_t segmentCount; /**< Valid entries of `segments`. */
	Segment segments[MAX_SEGMENTS]; /**< The loadable segments with something in memory, in file order. */
};

/**
 * @brief First page of a segment, as an address.
 * @param segment The segment.
 * @return The address rounded down to a page.
 */
inline uint32_t firstPage(const Segment &segment) { return segment.address & ~PAGE_MASK; }

/**
 * @brief Number of pages a segment covers.
 * @param segment A segment accepted by `parse`.
 * @return Pages from the one of its first byte to the one of its last.
 */
inline uint32_t pageCount(const Segment &segment) {
	uint64_t end = static_cast<uint64_t>(segment.address) + segment.memorySize;
	uint64_t last = (end + PAGE_MASK) & ~static_cast<uint64_t>(PAGE_MASK);
	return static_cast<uint32_t>((last - firstPage(segment)) / PAGE_BYTES);
}

/**
 * @brief Tells whether two segments share a page.
 * @param first A segment.
 * @param second Another segment.
 * @return True when some page holds bytes of both.
 */
inline bool sharePage(const Segment &first, const Segment &second) {
	uint64_t first_start = firstPage(first);
	uint64_t second_start = firstPage(second);
	uint64_t first_end = first_start + static_cast<uint64_t>(pageCount(first)) * PAGE_BYTES;
	uint64_t second_end = second_start + static_cast<uint64_t>(pageCount(second)) * PAGE_BYTES;
	return first_start < second_end && second_start < first_end;
}

/**
 * @brief Checks the file header of an image.
 * @param header Copy of the header.
 * @param size Size of the whole image.
 * @return True when class, encoding, version, type and machine are the ones of this
 * architecture, the program header entries have the right size, there is at least one and the
 * whole table lies inside the image.
 */
inline bool headerValid(const Header &header, size_t size) {
	for (size_t index = 0; index < MAGIC_SIZE; index++) {
		if (header.ident[index] != MAGIC[index]) return false;
	}
	if (header.ident[IDENT_CLASS] != abi::executable::ELF_CLASS) return false;
	if (header.ident[IDENT_DATA] != abi::executable::ELF_DATA_ENCODING) return false;
	if (header.ident[IDENT_VERSION] != VERSION_CURRENT) return false;
	if (header.type != TYPE_EXECUTABLE || header.machine != abi::executable::ELF_MACHINE) return false;
	if (header.programHeaderSize != PROGRAM_HEADER_SIZE || header.programHeaderCount == 0) return false;
	if (header.programHeaderOffset > size) return false;
	return header.programHeaderCount <= (size - header.programHeaderOffset) / PROGRAM_HEADER_SIZE;
}

/**
 * @brief Checks one loadable segment against the image and the address range.
 * @param program Copy of the program header.
 * @param size Size of the whole image.
 * @param imageLimit First address a segment may not reach.
 * @return True when the file size does not exceed the memory size, the contents lie inside the
 * image, and the segment starts above the first page and ends at or below the limit, with no
 * overflow.
 */
inline bool segmentValid(const ProgramHeader &program, size_t size, uint32_t imageLimit) {
	if (program.fileSize > program.memorySize) return false;
	if (program.offset > size || program.fileSize > size - program.offset) return false;
	if (program.address < RESERVED_LOW_BYTES) return false;
	if (program.memorySize > imageLimit) return false;
	return program.address <= imageLimit - program.memorySize;
}

/**
 * @brief Reads an image and lists what has to be loaded.
 * @param data Start of the image, untrusted.
 * @param size Size of the image in bytes.
 * @param imageLimit First address a segment may not reach: the user space limit minus the
 * user stack.
 * @param out Receives the entry point and the segments on success.
 * @return True when the image is a valid executable of this architecture, every loadable
 * segment is valid, no two segments share a page, there are at most `MAX_SEGMENTS` and the
 * entry point lies inside one of them. Segments with no bytes in memory are ignored.
 */
inline bool parse(const void *data, size_t size, uint32_t imageLimit, Image &out) {
	out = Image{};
	if (data == nullptr || size < sizeof(Header)) return false;

	const uint8_t *base = static_cast<const uint8_t *>(data);
	Header header;
	memcpy(&header, base, sizeof(header));
	if (!headerValid(header, size)) return false;

	bool entry_found = false;
	for (size_t index = 0; index < header.programHeaderCount; index++) {
		ProgramHeader program;
		memcpy(&program, base + header.programHeaderOffset + index * sizeof(program), sizeof(program));
		if (program.type != SEGMENT_LOAD || program.memorySize == 0) continue;
		if (!segmentValid(program, size, imageLimit) || out.segmentCount == MAX_SEGMENTS) return false;

		Segment &segment = out.segments[out.segmentCount];
		segment.address = program.address;
		segment.offset = program.offset;
		segment.fileSize = program.fileSize;
		segment.memorySize = program.memorySize;
		segment.writable = (program.flags & SEGMENT_WRITABLE) != 0;
		for (size_t before = 0; before < out.segmentCount; before++) {
			if (sharePage(out.segments[before], segment)) return false;
		}
		out.segmentCount++;
		if (header.entry >= program.address && header.entry - program.address < program.memorySize)
			entry_found = true;
	}
	out.entry = header.entry;
	return entry_found;
}

} // namespace cock::elf

#endif // !SHARED_COCK_ELF_IMAGE_HPP
