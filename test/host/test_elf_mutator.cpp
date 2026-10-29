#include <cock/abi/executable.hpp>
#include <cock/abi/process.hpp>
#include <cock/elf/image.hpp>
#include <test/host/check.hpp>
#include <test/userland/fuzz/elf_mutator.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using cock::elf::Image;
using cock::elf::ProgramHeader;
using fuzz::Prng;

namespace {

constexpr uint32_t SEED = 11;
constexpr uint32_t MUTANTS = 30000;
constexpr uint32_t IMAGE_LIMIT =
	cock::abi::executable::USER_SPACE_LIMIT - static_cast<uint32_t>(cock::abi::process::PENDING_STACK_BYTES);
constexpr uint32_t PAST_THE_FILE = 0x10000;

bool parseWithoutFileBounds(const void *data, size_t size, uint32_t imageLimit, Image &out) {
	out = Image{};
	if (data == nullptr || size < sizeof(cock::elf::Header)) return false;

	const uint8_t *base = static_cast<const uint8_t *>(data);
	cock::elf::Header header;
	memcpy(&header, base, sizeof(header));
	if (!cock::elf::headerValid(header, size)) return false;

	bool entryFound = false;
	for (size_t index = 0; index < header.programHeaderCount; index++) {
		ProgramHeader program;
		memcpy(&program, base + header.programHeaderOffset + index * sizeof(program), sizeof(program));
		if (program.type != cock::elf::SEGMENT_LOAD || program.memorySize == 0) continue;
		if (program.fileSize > program.memorySize || program.address < cock::elf::RESERVED_LOW_BYTES ||
			program.memorySize > imageLimit || program.address > imageLimit - program.memorySize ||
			out.segmentCount == cock::elf::MAX_SEGMENTS)
			return false;

		cock::elf::Segment &segment = out.segments[out.segmentCount];
		segment.address = program.address;
		segment.offset = program.offset;
		segment.fileSize = program.fileSize;
		segment.memorySize = program.memorySize;
		segment.writable = (program.flags & cock::elf::SEGMENT_WRITABLE) != 0;
		for (size_t before = 0; before < out.segmentCount; before++) {
			if (cock::elf::sharePage(out.segments[before], segment)) return false;
		}
		out.segmentCount++;
		if (header.entry >= program.address && header.entry - program.address < program.memorySize)
			entryFound = true;
	}
	out.entry = header.entry;
	return entryFound;
}

void checkSeedImageIsValid() {
	uint8_t image[fuzz::MUTANT_CAPACITY];
	size_t size = fuzz::buildSeedImage(image);
	Image parsed;

	CHECK_EQ(fuzz::SEED_IMAGE_BYTES, size);
	CHECK_TRUE(cock::elf::parse(image, size, IMAGE_LIMIT, parsed));
	CHECK_EQ(fuzz::SEED_PROGRAM_HEADERS, parsed.segmentCount);
	CHECK_EQ(fuzz::SEED_CODE_ADDRESS + static_cast<uint32_t>(fuzz::SEED_CODE_OFFSET), parsed.entry);
	CHECK_TRUE(fuzz::imageIsSafe(parsed, size, IMAGE_LIMIT));
}

void checkParserNeverAcceptsAnUnsafeMutant() {
	Prng prng(SEED);
	uint32_t accepted = 0;
	uint32_t rejected = 0;
	uint32_t unsafe = 0;
	for (uint32_t round = 0; round < MUTANTS; round++) {
		uint8_t image[fuzz::MUTANT_CAPACITY];
		size_t size = fuzz::mutateImage(prng, image, fuzz::buildSeedImage(image));
		Image parsed;
		if (!cock::elf::parse(image, size, IMAGE_LIMIT, parsed)) {
			rejected++;
			continue;
		}
		accepted++;
		if (!fuzz::imageIsSafe(parsed, size, IMAGE_LIMIT)) unsafe++;
	}
	CHECK_EQ(static_cast<uint32_t>(0), unsafe);
	CHECK_TRUE(accepted > 0);
	CHECK_TRUE(rejected > 0);
}

void checkBrokenBoundsCheckIsCaught() {
	Prng prng(SEED);
	uint32_t caught = 0;
	for (uint32_t round = 0; round < MUTANTS; round++) {
		uint8_t image[fuzz::MUTANT_CAPACITY];
		size_t size = fuzz::mutateImage(prng, image, fuzz::buildSeedImage(image));
		Image parsed;
		if (parseWithoutFileBounds(image, size, IMAGE_LIMIT, parsed) &&
			!fuzz::imageIsSafe(parsed, size, IMAGE_LIMIT))
			caught++;
	}
	CHECK_TRUE(caught > 0);
}

void checkUnsafeImagesAreRefused() {
	uint8_t image[fuzz::MUTANT_CAPACITY];
	size_t size = fuzz::buildSeedImage(image);
	Image parsed;
	cock::elf::parse(image, size, IMAGE_LIMIT, parsed);

	Image beyondFile = parsed;
	beyondFile.segments[0].fileSize = static_cast<uint32_t>(size) + PAST_THE_FILE;
	beyondFile.segments[0].memorySize = beyondFile.segments[0].fileSize;
	CHECK_FALSE(fuzz::imageIsSafe(beyondFile, size, IMAGE_LIMIT));

	Image beyondLimit = parsed;
	beyondLimit.segments[1].address = IMAGE_LIMIT;
	CHECK_FALSE(fuzz::imageIsSafe(beyondLimit, size, IMAGE_LIMIT));

	Image outsideEntry = parsed;
	outsideEntry.entry = 0;
	CHECK_FALSE(fuzz::imageIsSafe(outsideEntry, size, IMAGE_LIMIT));

	Image overlapping = parsed;
	overlapping.segments[1].address = overlapping.segments[0].address;
	CHECK_FALSE(fuzz::imageIsSafe(overlapping, size, IMAGE_LIMIT));
}

} // namespace

void testElfMutator() {
	checkSeedImageIsValid();
	checkParserNeverAcceptsAnUnsafeMutant();
	checkBrokenBoundsCheckIsCaught();
	checkUnsafeImagesAreRefused();
}

} // namespace host_test
