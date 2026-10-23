#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/loader/elf.hpp>
#include <cock/core/loader/elf32.hpp>
#include <cock/core/loader/elf64.hpp>
#include <cock/core/loader/elf_common.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/task/thread.hpp>
#include <hal/address_space.hpp>
#include <hal/executable.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <test/test_framework.hpp>

namespace cock::test::loader {

using core::loader::Elf32;
using core::loader::Elf64;
using core::loader::ELF_DATA_MSB;
using core::loader::ELF_IDENT_CLASS;
using core::loader::ELF_IDENT_DATA;
using core::loader::ELF_IDENT_MAGIC;
using core::loader::ELF_IDENT_VERSION;
using core::loader::ELF_MAGIC;
using core::loader::ELF_MAGIC_SIZE;
using core::loader::ELF_PF_WRITE;
using core::loader::ELF_PT_LOAD;
using core::loader::ELF_TYPE_DYN;
using core::loader::ELF_TYPE_EXEC;
using core::loader::ELF_VERSION_CURRENT;
using core::loader::load_executable;
using core::loader::LoadedImage;
using core::memory::PhysAddr;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;

namespace {

constexpr size_t PAYLOAD_SIZE = 16;
constexpr size_t MEMORY_SIZE = 0x2000;
constexpr size_t SMALLER_MEMORY_SIZE = PAYLOAD_SIZE / 2;
constexpr size_t PAYLOAD_PATTERN_BASE = 0xA0;
constexpr size_t TRUNCATED_IDENT_SIZE = PAYLOAD_SIZE - 1;
constexpr size_t SINGLE_PAGE = 1;
constexpr size_t TWO_PAGES = 2;
constexpr uintptr_t NULL_ADDRESS = 0;
constexpr uintptr_t CODE_ADDRESS = 0x400000;
constexpr uintptr_t READONLY_ADDRESS = 0x800000;
constexpr uintptr_t PER_PROCESS_LIMIT = 0x700000;
constexpr uint8_t UNKNOWN_CLASS = 3;
constexpr uint8_t CORRUPTED_BYTE = 0;
constexpr uint16_t EXCESSIVE_SEGMENT_COUNT = 0x100;
constexpr uint16_t NO_SEGMENTS = 0;
constexpr uint16_t SINGLE_SEGMENT = 1;
constexpr size_t TWO_SEGMENTS = 2;
constexpr uint16_t BAD_ENTRY_SIZE = 1;
constexpr uint16_t MACHINE_STEP = 1;
constexpr uint32_t FLAG_READ = 4;
constexpr uint32_t FLAGS_READ_WRITE = ELF_PF_WRITE | FLAG_READ;
constexpr uint32_t HEADER_VERSION = 1;

bool user_range_ok(uintptr_t ptr, size_t length, bool write, uintptr_t limit) {
	return VirtualMemoryManager::isUserRangeValid(VirtAddr(ptr), length, write,
												  VirtAddr(limit));
}

template <typename Format> struct File {
	typename Format::Header header;
	typename Format::ProgramHeader segment;
	uint8_t payload[PAYLOAD_SIZE];
};

template <typename Format>
void make_valid(File<Format> &file, uintptr_t vaddr, uint32_t flags) {
	using Header = typename Format::Header;
	using Segment = typename Format::ProgramHeader;

	memset(&file, 0, sizeof(file));
	memcpy(file.header.ident, ELF_MAGIC, ELF_MAGIC_SIZE);
	file.header.ident[ELF_IDENT_CLASS] = Format::CLASS_ID;
	file.header.ident[ELF_IDENT_DATA] = core::hal::ELF_DATA_ENCODING;
	file.header.ident[ELF_IDENT_VERSION] = ELF_VERSION_CURRENT;
	file.header.type = ELF_TYPE_EXEC;
	file.header.machine = Format::MACHINE;
	file.header.version = HEADER_VERSION;
	file.header.entry = vaddr;
	file.header.program_header_offset = sizeof(Header);
	file.header.header_size = sizeof(Header);
	file.header.program_header_entry_size = sizeof(Segment);
	file.header.program_header_num = SINGLE_SEGMENT;

	file.segment.type = ELF_PT_LOAD;
	file.segment.flags = flags;
	file.segment.offset = sizeof(Header) + sizeof(Segment);
	file.segment.vaddr = vaddr;
	file.segment.size_in_file = PAYLOAD_SIZE;
	file.segment.size_in_memory = MEMORY_SIZE;
	file.segment.align = core::hal::PAGE_SIZE;

	for (size_t idx = 0; idx < PAYLOAD_SIZE; idx++)
		file.payload[idx] = static_cast<uint8_t>(PAYLOAD_PATTERN_BASE + idx);
}

template <typename Format> struct FileWithEmptySegment {
	typename Format::Header header;
	typename Format::ProgramHeader segments[TWO_SEGMENTS];
	uint8_t payload[PAYLOAD_SIZE];
};

template <typename Format>
bool loads(const File<Format> &file, PhysAddr address_space,
		   LoadedImage &out) {
	return load_executable(&file, sizeof(file), address_space, out);
}

template <typename Format>
bool rejected(const File<Format> &file, PhysAddr address_space,
			  size_t size = sizeof(File<Format>)) {
	LoadedImage out;
	bool result = load_executable(&file, size, address_space, out);
	return !result && out.entry.isNull() && out.user_space_limit.isNull();
}

template <typename Format> void check_rejections(PhysAddr as) {
	constexpr uintptr_t limit = Format::USER_SPACE_LIMIT;
	constexpr uintptr_t image_limit = limit - core::task::USER_STACK_SIZE;
	File<Format> file;

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.header.ident[ELF_IDENT_MAGIC] = CORRUPTED_BYTE;
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	EXPECT_TRUE(rejected(file, as, TRUNCATED_IDENT_SIZE));
	EXPECT_TRUE(rejected(file, as, sizeof(typename Format::Header) - 1));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.header.machine = static_cast<uint16_t>(Format::MACHINE + MACHINE_STEP);
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.header.type = ELF_TYPE_DYN;
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.header.ident[ELF_IDENT_DATA] = ELF_DATA_MSB;
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.header.entry = CODE_ADDRESS + MEMORY_SIZE;
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.header.program_header_num = EXCESSIVE_SEGMENT_COUNT;
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.header.program_header_num = NO_SEGMENTS;
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.header.program_header_entry_size = BAD_ENTRY_SIZE;
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, NULL_ADDRESS, FLAGS_READ_WRITE);
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, limit, FLAGS_READ_WRITE);
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, image_limit - core::hal::PAGE_SIZE, FLAGS_READ_WRITE);
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.segment.size_in_file = MEMORY_SIZE + SINGLE_PAGE;
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.segment.size_in_memory = SMALLER_MEMORY_SIZE;
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.segment.offset = sizeof(file);
	EXPECT_TRUE(rejected(file, as));

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	file.segment.size_in_memory =
		static_cast<decltype(file.segment.size_in_memory)>(UINT64_MAX);
	EXPECT_TRUE(rejected(file, as));
}

template <typename Format> void check_empty_segment(PhysAddr as) {
	using Header = typename Format::Header;
	using Segment = typename Format::ProgramHeader;

	File<Format> base;
	make_valid(base, CODE_ADDRESS, FLAGS_READ_WRITE);

	FileWithEmptySegment<Format> file;
	memset(&file, 0, sizeof(file));
	file.header = base.header;
	file.header.program_header_num = TWO_SEGMENTS;
	file.segments[0] = base.segment;
	file.segments[0].offset = sizeof(Header) + TWO_SEGMENTS * sizeof(Segment);
	file.segments[1].type = ELF_PT_LOAD;
	file.segments[1].flags = FLAGS_READ_WRITE;
	memcpy(file.payload, base.payload, PAYLOAD_SIZE);

	LoadedImage out;
	EXPECT_TRUE(load_executable(&file, sizeof(file), as, out));
	EXPECT_EQ(CODE_ADDRESS, out.entry.raw());

	file.segments[1].size_in_file = SINGLE_PAGE;
	EXPECT_FALSE(load_executable(&file, sizeof(file), as, out));
}

template <typename Format> void check_load(PhysAddr as) {
	constexpr uintptr_t limit = Format::USER_SPACE_LIMIT;
	File<Format> file;
	LoadedImage out;

	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	EXPECT_TRUE(loads(file, as, out));
	EXPECT_EQ(CODE_ADDRESS, out.entry.raw());
	EXPECT_EQ(limit, out.user_space_limit.raw());

	PhysAddr old_cr3 = core::hal::get_current_address_space();
	core::hal::switch_address_space(as);

	bool writable = user_range_ok(
		CODE_ADDRESS, MEMORY_SIZE, true, limit);
	bool intact = writable;
	if (writable) {
		const uint8_t *mem = reinterpret_cast<const uint8_t *>(CODE_ADDRESS);
		for (size_t idx = 0; idx < PAYLOAD_SIZE; idx++)
			intact = intact && mem[idx] == file.payload[idx];
		for (size_t idx = PAYLOAD_SIZE; idx < MEMORY_SIZE; idx++)
			intact = intact && mem[idx] == 0;
	}

	core::hal::switch_address_space(old_cr3);
	EXPECT_TRUE(writable);
	EXPECT_TRUE(intact);

	make_valid(file, READONLY_ADDRESS, FLAG_READ);
	EXPECT_TRUE(loads(file, as, out));
	EXPECT_EQ(READONLY_ADDRESS, out.entry.raw());

	core::hal::switch_address_space(as);
	bool read_ok = user_range_ok(
		READONLY_ADDRESS, MEMORY_SIZE, false, limit);
	bool write_ok = user_range_ok(
		READONLY_ADDRESS, MEMORY_SIZE, true, limit);
	bool under_small_limit = user_range_ok(
		READONLY_ADDRESS, PAYLOAD_SIZE, false, PER_PROCESS_LIMIT);
	bool crossing_limit = user_range_ok(
		limit - core::hal::PAGE_SIZE, TWO_PAGES * core::hal::PAGE_SIZE, false,
		limit);
	core::hal::switch_address_space(old_cr3);

	EXPECT_TRUE(read_ok);
	EXPECT_FALSE(write_ok);
	EXPECT_FALSE(under_small_limit);
	EXPECT_FALSE(crossing_limit);
}

template <typename Small, typename Large>
void check_class_limits(PhysAddr as) {
	constexpr uintptr_t small_limit = Small::USER_SPACE_LIMIT;
	constexpr uintptr_t middle =
		small_limit - core::task::USER_STACK_SIZE + core::hal::PAGE_SIZE;
	File<Small> small_file;
	File<Large> large_file;
	LoadedImage out;

	make_valid(small_file, middle, FLAGS_READ_WRITE);
	EXPECT_TRUE(rejected(small_file, as));

	make_valid(large_file, middle, FLAGS_READ_WRITE);
	EXPECT_TRUE(loads(large_file, as, out));
	EXPECT_EQ(Large::USER_SPACE_LIMIT, out.user_space_limit.raw());
}

template <typename Format> void check_unsupported(PhysAddr as) {
	File<Format> file;
	make_valid(file, CODE_ADDRESS, FLAGS_READ_WRITE);
	EXPECT_TRUE(rejected(file, as));
}

} // namespace

void test_elf_loader() {
	TestRunner::begin_suite("ELF loader (ELF32 / ELF64)");

	PhysAddr as = VirtualMemoryManager::createAddressSpace();
	EXPECT_FALSE(as.isNull());
	if (as.isNull()) {
		TestRunner::end_suite();
		return;
	}

	core::hal::InterruptGuard guard;

	LoadedImage out;
	File<Elf32> probe;
	make_valid(probe, CODE_ADDRESS, FLAGS_READ_WRITE);
	EXPECT_FALSE(load_executable(nullptr, sizeof(probe), as, out));
	EXPECT_FALSE(load_executable(&probe, sizeof(probe), PhysAddr(), out));
	probe.header.ident[ELF_IDENT_CLASS] = UNKNOWN_CLASS;
	EXPECT_TRUE(rejected(probe, as));

	if constexpr (Elf32::SUPPORTED) {
		check_rejections<Elf32>(as);
		check_empty_segment<Elf32>(as);
		check_load<Elf32>(as);
	} else check_unsupported<Elf32>(as);

	if constexpr (Elf64::SUPPORTED) {
		check_rejections<Elf64>(as);
		check_empty_segment<Elf64>(as);
		check_load<Elf64>(as);
	} else check_unsupported<Elf64>(as);

	if constexpr (Elf32::SUPPORTED && Elf64::SUPPORTED) {
		if constexpr (Elf64::USER_SPACE_LIMIT < Elf32::USER_SPACE_LIMIT)
			check_class_limits<Elf64, Elf32>(as);
		else if constexpr (Elf32::USER_SPACE_LIMIT < Elf64::USER_SPACE_LIMIT)
			check_class_limits<Elf32, Elf64>(as);
	}

	VirtualMemoryManager::destroyAddressSpace(as);
	TestRunner::end_suite();
}

} // namespace cock::test::loader
