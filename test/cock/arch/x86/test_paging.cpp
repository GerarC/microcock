#include <cock/arch/x86/memory/layout.hpp>
#include <cock/arch/x86/memory/paging.hpp>
#include <cock/core/memory/vmm.hpp>
#include <hal/cpu.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

extern "C" char _text_start[];
extern "C" char _rodata_start[];

namespace cock::test::arch {

using cock::arch::x86::PageFlag;
using cock::arch::x86::Paging;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;

namespace {

int writableData = 1;

bool present(uint32_t flags) { return (flags & PageFlag::PRESENT) != 0; }

bool writable(uint32_t flags) { return (flags & PageFlag::WRITEABLE) != 0; }

void checkKernelImageProtection() {
	EXPECT_TRUE((core::hal::read_cr0() & core::hal::CR0_WRITE_PROTECT) != 0);

	uint32_t text = Paging::getPageFlags(VirtAddr::of(_text_start));
	EXPECT_TRUE(present(text));
	EXPECT_FALSE(writable(text));

	uint32_t constants = Paging::getPageFlags(VirtAddr::of(_rodata_start));
	EXPECT_TRUE(present(constants));
	EXPECT_FALSE(writable(constants));

	uint32_t data = Paging::getPageFlags(VirtAddr::of(&writableData));
	EXPECT_TRUE(present(data));
	EXPECT_TRUE(writable(data));

	void *heap_page = VirtualMemoryManager::allocPages(1);
	EXPECT_NOT_NULL(heap_page);
	if (heap_page == nullptr) return;
	EXPECT_TRUE(writable(Paging::getPageFlags(VirtAddr::of(heap_page))));
	VirtualMemoryManager::freePages(heap_page, 1);
}

void checkFpuIsDisabled() {
	EXPECT_TRUE((core::hal::read_cr0() & core::hal::CR0_EMULATE_FPU) != 0);
}

void checkProtectPage() {
	void *heap_page = VirtualMemoryManager::allocPages(1);
	EXPECT_NOT_NULL(heap_page);
	if (heap_page == nullptr) return;

	VirtAddr page = VirtAddr::of(heap_page);
	EXPECT_TRUE(writable(Paging::getPageFlags(page)));
	EXPECT_TRUE(Paging::updatePageFlags(page, PageFlag::PRESENT));
	EXPECT_FALSE(writable(Paging::getPageFlags(page)));
	EXPECT_TRUE(Paging::updatePageFlags(page, PageFlag::PRESENT | PageFlag::WRITEABLE));
	EXPECT_TRUE(writable(Paging::getPageFlags(page)));
	VirtualMemoryManager::freePages(heap_page, 1);
}

} // namespace

void test_paging() {
	TestRunner::begin_suite("Paging protections");
	checkKernelImageProtection();
	checkFpuIsDisabled();
	checkProtectPage();
	TestRunner::end_suite();
}

} // namespace cock::test::arch
