#include <cock/abi/executable.hpp>
#include <cock/elf/image.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using cock::elf::Header;
using cock::elf::Image;
using cock::elf::ProgramHeader;

namespace {

constexpr size_t PROGRAM_HEADERS = 3;
constexpr size_t LOADED_SEGMENTS = 2;
constexpr uint32_t CODE_PAGES = 1;
constexpr uint32_t DATA_PAGES = 3;
constexpr size_t CONTENT_BYTES = 64;
constexpr uint32_t IMAGE_LIMIT = 0xBFFF0000;
constexpr uint32_t CODE_ADDRESS = 0x400000;
constexpr uint32_t DATA_ADDRESS = 0x402000;
constexpr uint32_t CODE_MEMORY = 0x100;
constexpr uint32_t DATA_MEMORY = 0x3000;
constexpr uint32_t SHARED_PAGE_ADDRESS = 0x400800;
constexpr uint32_t FIRST_PAGE_ADDRESS = 0x800;
constexpr uint32_t WRAPPING_SIZE = 0xFFFFFFF0;
constexpr uint32_t NOT_A_HEADER_SIZE = 8;
constexpr uint32_t SEGMENT_FLAGS_CODE = 5;
constexpr uint16_t DYNAMIC_TYPE = 3;
constexpr uint16_t OTHER_MACHINE = 62;
constexpr uint8_t OTHER_CLASS = 2;
constexpr uint8_t BIG_ENDIAN_DATA = 2;
constexpr uint8_t BAD_MAGIC_BYTE = 0x7E;
constexpr uint32_t PAGE_AND_A_HALF = 0x1800;
constexpr uint32_t TWO_PAGES = 0x2000;
constexpr uint32_t BYTES_PAST_TWO_PAGES = 0x2001;
constexpr size_t OVER_SEGMENT_LIMIT = cock::elf::MAX_SEGMENTS + 1;

struct File {
	Header header;
	ProgramHeader programs[PROGRAM_HEADERS];
	uint8_t content[CONTENT_BYTES];
};

File makeFile() {
	File file = {};
	memcpy(file.header.ident, cock::elf::MAGIC, cock::elf::MAGIC_SIZE);
	file.header.ident[cock::elf::IDENT_CLASS] = cock::abi::executable::ELF_CLASS;
	file.header.ident[cock::elf::IDENT_DATA] = cock::abi::executable::ELF_DATA_ENCODING;
	file.header.ident[cock::elf::IDENT_VERSION] = cock::elf::VERSION_CURRENT;
	file.header.type = cock::elf::TYPE_EXECUTABLE;
	file.header.machine = cock::abi::executable::ELF_MACHINE;
	file.header.entry = CODE_ADDRESS;
	file.header.programHeaderOffset = offsetof(File, programs);
	file.header.programHeaderSize = cock::elf::PROGRAM_HEADER_SIZE;
	file.header.programHeaderCount = LOADED_SEGMENTS;

	file.programs[0].type = cock::elf::SEGMENT_LOAD;
	file.programs[0].offset = offsetof(File, content);
	file.programs[0].address = CODE_ADDRESS;
	file.programs[0].fileSize = CONTENT_BYTES;
	file.programs[0].memorySize = CODE_MEMORY;
	file.programs[0].flags = SEGMENT_FLAGS_CODE;

	file.programs[1].type = cock::elf::SEGMENT_LOAD;
	file.programs[1].offset = offsetof(File, content);
	file.programs[1].address = DATA_ADDRESS;
	file.programs[1].fileSize = 0;
	file.programs[1].memorySize = DATA_MEMORY;
	file.programs[1].flags = cock::elf::SEGMENT_WRITABLE;
	return file;
}

bool parses(const File &file, Image &out, size_t size = sizeof(File)) {
	return cock::elf::parse(&file, size, IMAGE_LIMIT, out);
}

void checkValidImage() {
	File file = makeFile();
	Image image;
	CHECK_TRUE(parses(file, image));
	CHECK_EQ(LOADED_SEGMENTS, image.segmentCount);
	CHECK_EQ(CODE_ADDRESS, image.entry);
	CHECK_FALSE(image.segments[0].writable);
	CHECK_TRUE(image.segments[1].writable);
	CHECK_EQ(CODE_PAGES, cock::elf::pageCount(image.segments[0]));
	CHECK_EQ(DATA_PAGES, cock::elf::pageCount(image.segments[1]));
	CHECK_EQ(DATA_ADDRESS, cock::elf::firstPage(image.segments[1]));
}

void checkEmptySegmentIsIgnored() {
	File file = makeFile();
	file.header.programHeaderCount = PROGRAM_HEADERS;
	file.programs[2].type = cock::elf::SEGMENT_LOAD;
	file.programs[2].address = 0;
	file.programs[2].memorySize = 0;
	Image image;
	CHECK_TRUE(parses(file, image));
	CHECK_EQ(LOADED_SEGMENTS, image.segmentCount);
}

void checkHeaderRejections() {
	Image image;
	File file = makeFile();
	CHECK_FALSE(cock::elf::parse(nullptr, sizeof(file), IMAGE_LIMIT, image));
	CHECK_FALSE(cock::elf::parse(&file, NOT_A_HEADER_SIZE, IMAGE_LIMIT, image));

	file = makeFile();
	file.header.ident[0] = BAD_MAGIC_BYTE;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.ident[cock::elf::IDENT_CLASS] = OTHER_CLASS;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.ident[cock::elf::IDENT_DATA] = BIG_ENDIAN_DATA;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.type = DYNAMIC_TYPE;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.machine = OTHER_MACHINE;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.programHeaderSize = 0;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.programHeaderCount = 0;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.programHeaderOffset = sizeof(File);
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.programHeaderCount = PROGRAM_HEADERS + 1;
	CHECK_FALSE(parses(file, image, offsetof(File, content)));
}

void checkSegmentRejections() {
	Image image;
	File file = makeFile();
	file.programs[0].fileSize = CODE_MEMORY + 1;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.programs[0].offset = sizeof(File);
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.programs[0].fileSize = CONTENT_BYTES + 1;
	file.programs[0].memorySize = CODE_MEMORY;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.programs[0].address = FIRST_PAGE_ADDRESS;
	file.header.entry = FIRST_PAGE_ADDRESS;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.programs[1].address = IMAGE_LIMIT - CODE_MEMORY + 1;
	file.programs[1].memorySize = CODE_MEMORY;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.programs[1].address = IMAGE_LIMIT;
	file.programs[1].memorySize = WRAPPING_SIZE;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.programs[1].address = DATA_ADDRESS;
	file.programs[1].memorySize = WRAPPING_SIZE;
	CHECK_FALSE(parses(file, image));
}

void checkEntryRejections() {
	Image image;
	File file = makeFile();
	file.header.entry = CODE_ADDRESS + CODE_MEMORY;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.entry = 0;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.header.entry = DATA_ADDRESS;
	CHECK_TRUE(parses(file, image));
}

void checkSharedPages() {
	Image image;
	File file = makeFile();
	file.programs[1].address = SHARED_PAGE_ADDRESS;
	file.programs[1].memorySize = CODE_MEMORY;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.programs[1].address = DATA_ADDRESS - PAGE_AND_A_HALF;
	file.programs[1].memorySize = TWO_PAGES;
	CHECK_FALSE(parses(file, image));

	file = makeFile();
	file.programs[0].memorySize = BYTES_PAST_TWO_PAGES;
	file.programs[0].fileSize = CONTENT_BYTES;
	file.programs[1].address = DATA_ADDRESS;
	CHECK_FALSE(parses(file, image));
}

void checkSegmentLimit() {
	constexpr size_t TABLE = cock::elf::MAX_SEGMENTS + 2;
	struct Many {
		Header header;
		ProgramHeader programs[TABLE];
	} many = {};
	File base = makeFile();
	many.header = base.header;
	many.header.programHeaderOffset = offsetof(Many, programs);
	many.header.programHeaderCount = OVER_SEGMENT_LIMIT;
	for (size_t index = 0; index < OVER_SEGMENT_LIMIT; index++) {
		many.programs[index].type = cock::elf::SEGMENT_LOAD;
		many.programs[index].address = CODE_ADDRESS + static_cast<uint32_t>(index) * TWO_PAGES;
		many.programs[index].memorySize = CODE_MEMORY;
	}
	Image image;
	CHECK_FALSE(cock::elf::parse(&many, sizeof(many), IMAGE_LIMIT, image));
	many.header.programHeaderCount = cock::elf::MAX_SEGMENTS;
	CHECK_TRUE(cock::elf::parse(&many, sizeof(many), IMAGE_LIMIT, image));
	CHECK_EQ(cock::elf::MAX_SEGMENTS, image.segmentCount);
}

} // namespace

void testElfImage() {
	checkValidImage();
	checkEmptySegmentIsIgnored();
	checkHeaderRejections();
	checkSegmentRejections();
	checkEntryRejections();
	checkSharedPages();
	checkSegmentLimit();
}

} // namespace host_test
