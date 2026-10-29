#include "elf_mutator.hpp"
#include <cock/abi/executable.hpp>
#include <string.h>

namespace fuzz {

using cock::elf::Header;
using cock::elf::Image;
using cock::elf::ProgramHeader;

namespace {

constexpr size_t WORD_BYTES = sizeof(uint32_t);
constexpr size_t HALF_WORD_BYTES = sizeof(uint16_t);
constexpr uint32_t BITS_PER_BYTE = 8;
constexpr uint32_t BYTE_VALUES = 256;
constexpr uint32_t MAX_MUTATIONS = 3;
constexpr uint32_t SEGMENT_EXECUTABLE = 1;
constexpr uint32_t SEGMENT_READABLE = 4;
constexpr uint32_t SEED_CODE_FLAGS = SEGMENT_READABLE | SEGMENT_EXECUTABLE;
constexpr uint32_t SEED_DATA_FLAGS = SEGMENT_READABLE | cock::elf::SEGMENT_WRITABLE;
constexpr uint32_t TABLE_OFFSET_PERCENT = 50;
constexpr uint32_t SEED_ALIGNMENT = cock::elf::PAGE_BYTES;
constexpr size_t TABLE_WORDS = (cock::elf::HEADER_SIZE + SEED_PROGRAM_HEADERS * cock::elf::PROGRAM_HEADER_SIZE) / WORD_BYTES;
constexpr uint16_t HOSTILE_COUNTS[] = {0, 1, 2, 3, 0x100, 0xFFFF};
constexpr uint32_t HOSTILE_WORDS[] = {
	0x0,        0x1,        0xFFF,      0x1000,     0xFFFF,     0x10000,
	0x7FFFFFFF, 0x80000000, 0xBFFFF000, 0xBFFFFFFF, 0xC0000000, 0xFFFFF000,
	0xFFFFFFFE, 0xFFFFFFFF, SEED_IMAGE_BYTES, SEED_IMAGE_BYTES - 1, SEED_IMAGE_BYTES + 1};

constexpr uint8_t SEED_CODE[SEED_CODE_BYTES] = {OPCODE_MOVE_EAX, SEED_EXIT_CALL, ZERO_BYTE,
												ZERO_BYTE,       ZERO_BYTE,      OPCODE_XOR,
												MODRM_EBX_EBX,   OPCODE_INTERRUPT, SYSCALL_VECTOR};

enum class Mutation : uint32_t {
	FLIP_BIT,
	SET_BYTE,
	HOSTILE_TABLE_WORD,
	HOSTILE_ANY_WORD,
	SEGMENT_OFFSET,
	INFLATE_SEGMENT,
	RESIZE_IMAGE,
	COPY_PROGRAM_HEADER,
	TABLE_SHAPE,
	COUNT,
};

size_t programHeaderOffset(uint32_t index) {
	return cock::elf::HEADER_SIZE + static_cast<size_t>(index) * cock::elf::PROGRAM_HEADER_SIZE;
}

void writeBytes(uint8_t *image, size_t size, size_t offset, const void *value, size_t bytes) {
	if (size < bytes) return;
	if (offset > size - bytes) offset = size - bytes;
	memcpy(image + offset, value, bytes);
}

void writeWord(uint8_t *image, size_t size, size_t offset, uint32_t value) {
	writeBytes(image, size, offset, &value, WORD_BYTES);
}

uint32_t hostileWord(Prng &prng) { return prng.pick(HOSTILE_WORDS); }

size_t resize(Prng &prng, uint8_t *image, size_t size) {
	constexpr uint32_t KINDS = 5;
	size_t wanted = size;
	switch (prng.below(KINDS)) {
	case 0:
		wanted = 0;
		break;
	case 1:
		wanted = cock::elf::HEADER_SIZE - 1;
		break;
	case 2:
		wanted = cock::elf::HEADER_SIZE + prng.below(SEED_PROGRAM_HEADERS * cock::elf::PROGRAM_HEADER_SIZE);
		break;
	case 3:
		wanted = prng.below(static_cast<uint32_t>(size) + 1);
		break;
	default:
		wanted = size + prng.below(MUTANT_EXTRA_BYTES + 1);
		break;
	}
	if (wanted > MUTANT_CAPACITY) wanted = MUTANT_CAPACITY;
	if (wanted > size) memset(image + size, ZERO_BYTE, wanted - size);
	return wanted;
}

void applyOne(Prng &prng, uint8_t *image, size_t &size) {
	Mutation kind = static_cast<Mutation>(prng.below(static_cast<uint32_t>(Mutation::COUNT)));
	uint32_t index = prng.below(SEED_PROGRAM_HEADERS);
	size_t segment = programHeaderOffset(index);

	switch (kind) {
	case Mutation::FLIP_BIT:
		if (size != 0) image[prng.below(static_cast<uint32_t>(size))] ^= static_cast<uint8_t>(1U << prng.below(BITS_PER_BYTE));
		break;
	case Mutation::SET_BYTE:
		if (size != 0) image[prng.below(static_cast<uint32_t>(size))] = static_cast<uint8_t>(prng.below(BYTE_VALUES));
		break;
	case Mutation::HOSTILE_TABLE_WORD:
		writeWord(image, size, prng.below(TABLE_WORDS) * WORD_BYTES, hostileWord(prng));
		break;
	case Mutation::HOSTILE_ANY_WORD:
		writeWord(image, size, prng.below(static_cast<uint32_t>(size) + 1), hostileWord(prng));
		break;
	case Mutation::SEGMENT_OFFSET:
		writeWord(image, size, segment + offsetof(ProgramHeader, offset), hostileWord(prng));
		break;
	case Mutation::INFLATE_SEGMENT: {
		uint32_t bytes = hostileWord(prng);
		writeWord(image, size, segment + offsetof(ProgramHeader, fileSize), bytes);
		writeWord(image, size, segment + offsetof(ProgramHeader, memorySize), bytes);
		break;
	}
	case Mutation::RESIZE_IMAGE:
		size = resize(prng, image, size);
		break;
	case Mutation::COPY_PROGRAM_HEADER: {
		size_t other = programHeaderOffset((index + 1) % SEED_PROGRAM_HEADERS);
		if (size >= other + cock::elf::PROGRAM_HEADER_SIZE && size >= segment + cock::elf::PROGRAM_HEADER_SIZE)
			memcpy(image + other, image + segment, cock::elf::PROGRAM_HEADER_SIZE);
		break;
	}
	case Mutation::TABLE_SHAPE: {
		uint16_t count = prng.pick(HOSTILE_COUNTS);
		writeBytes(image, size, offsetof(Header, programHeaderCount), &count, HALF_WORD_BYTES);
		if (prng.chance(TABLE_OFFSET_PERCENT))
			writeWord(image, size, offsetof(Header, programHeaderOffset), hostileWord(prng));
		break;
	}
	case Mutation::COUNT:
		break;
	}
}

} // namespace

size_t buildSeedImage(uint8_t *out) {
	memset(out, ZERO_BYTE, MUTANT_CAPACITY);

	Header header = {};
	memcpy(header.ident, cock::elf::MAGIC, cock::elf::MAGIC_SIZE);
	header.ident[cock::elf::IDENT_CLASS] = cock::abi::executable::ELF_CLASS;
	header.ident[cock::elf::IDENT_DATA] = cock::abi::executable::ELF_DATA_ENCODING;
	header.ident[cock::elf::IDENT_VERSION] = cock::elf::VERSION_CURRENT;
	header.type = cock::elf::TYPE_EXECUTABLE;
	header.machine = cock::abi::executable::ELF_MACHINE;
	header.version = cock::elf::VERSION_CURRENT;
	header.entry = SEED_CODE_ADDRESS + SEED_CODE_OFFSET;
	header.programHeaderOffset = cock::elf::HEADER_SIZE;
	header.headerSize = cock::elf::HEADER_SIZE;
	header.programHeaderSize = cock::elf::PROGRAM_HEADER_SIZE;
	header.programHeaderCount = SEED_PROGRAM_HEADERS;
	memcpy(out, &header, sizeof(header));

	ProgramHeader code = {};
	code.type = cock::elf::SEGMENT_LOAD;
	code.offset = 0;
	code.address = SEED_CODE_ADDRESS;
	code.physical = SEED_CODE_ADDRESS;
	code.fileSize = SEED_IMAGE_BYTES;
	code.memorySize = SEED_IMAGE_BYTES;
	code.flags = SEED_CODE_FLAGS;
	code.alignment = SEED_ALIGNMENT;
	memcpy(out + programHeaderOffset(0), &code, sizeof(code));

	ProgramHeader data = {};
	data.type = cock::elf::SEGMENT_LOAD;
	data.offset = SEED_IMAGE_BYTES;
	data.address = SEED_DATA_ADDRESS;
	data.physical = SEED_DATA_ADDRESS;
	data.fileSize = 0;
	data.memorySize = SEED_DATA_BYTES;
	data.flags = SEED_DATA_FLAGS;
	data.alignment = SEED_ALIGNMENT;
	memcpy(out + programHeaderOffset(1), &data, sizeof(data));

	memcpy(out + SEED_CODE_OFFSET, SEED_CODE, SEED_CODE_BYTES);
	return SEED_IMAGE_BYTES;
}

size_t mutateImage(Prng &prng, uint8_t *image, size_t size) {
	uint32_t count = prng.below(MAX_MUTATIONS) + 1;
	for (uint32_t step = 0; step < count; step++)
		applyOne(prng, image, size);
	return size;
}

bool imageIsSafe(const Image &image, size_t size, uint32_t limit) {
	if (image.segmentCount > cock::elf::MAX_SEGMENTS) return false;

	bool entryInside = false;
	for (size_t index = 0; index < image.segmentCount; index++) {
		const cock::elf::Segment &segment = image.segments[index];
		uint64_t memoryEnd = static_cast<uint64_t>(segment.address) + segment.memorySize;
		uint64_t fileEnd = static_cast<uint64_t>(segment.offset) + segment.fileSize;
		if (segment.fileSize > segment.memorySize || segment.memorySize == 0) return false;
		if (fileEnd > size) return false;
		if (segment.address < cock::elf::RESERVED_LOW_BYTES || memoryEnd > limit) return false;
		for (size_t before = 0; before < index; before++) {
			if (cock::elf::sharePage(image.segments[before], segment)) return false;
		}
		if (image.entry >= segment.address && image.entry < memoryEnd) entryInside = true;
	}
	return entryInside;
}

} // namespace fuzz
