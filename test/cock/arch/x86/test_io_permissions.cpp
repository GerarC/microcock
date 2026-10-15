#include <cock/arch/x86/gdt/gdt.hpp>
#include <cock/core/hal/tasking.hpp>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::arch {

using core::hal::ArchProcessContext;
using cock::arch::x86::GDT;

namespace {

constexpr uint32_t KEYBOARD_DATA_PORT = 0x60;
constexpr uint32_t SERIAL_FIRST_PORT = 0x3F8;
constexpr uint32_t SERIAL_PORT_COUNT = 8;
constexpr uint32_t SINGLE_PORT = 1;

} // namespace

void test_io_permissions() {
	TestRunner::begin_suite("Lazy I/O permission map");

	static ArchProcessContext first;
	static ArchProcessContext second;
	core::hal::init_process_context(first);
	core::hal::init_process_context(second);
	core::hal::grant_hardware_resource(first, KEYBOARD_DATA_PORT, SINGLE_PORT);
	core::hal::grant_hardware_resource(second, SERIAL_FIRST_PORT,
									   SERIAL_PORT_COUNT);

	core::hal::apply_process_context(first);
	EXPECT_EQ(first.version, GDT::appliedIoVersion());

	core::hal::apply_process_context(second);
	EXPECT_EQ(second.version, GDT::appliedIoVersion());

	core::hal::apply_process_context(first);
	EXPECT_EQ(first.version, GDT::appliedIoVersion());

	core::hal::grant_hardware_resource(first, SERIAL_FIRST_PORT,
									   SERIAL_PORT_COUNT);
	EXPECT_TRUE(first.version != GDT::appliedIoVersion());
	core::hal::apply_process_context(first);
	EXPECT_EQ(first.version, GDT::appliedIoVersion());

	TestRunner::end_suite();
}

} // namespace cock::test::arch
