#include <cock/core/irq/irq_routing.hpp>
#include <cock/core/security/capability.hpp>
#include <hal/interrupt.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::security {

using core::irq::IrqRouting;
using core::security::Capability;
using core::security::CAPABILITY_FLAG_GRANT;
using core::security::CapabilitySet;
using core::security::CapabilityType;

namespace {

constexpr uint64_t PORTS_START = 0x60;
constexpr uint64_t PORTS_COUNT = 2;
constexpr uint64_t SINGLE = 1;
constexpr uint64_t LINE_COUNT = 4;
constexpr uint64_t FIRST_LINE = 1;
constexpr uint64_t PAST_THE_END = UINT64_MAX;
constexpr uint32_t FIRST_OWNER = 40;
constexpr uint32_t SECOND_OWNER = 41;
constexpr uint32_t KEYBOARD_LINE = 1;
constexpr uint32_t UNBOUND_LINE = 3;
constexpr uint32_t OUT_OF_RANGE_LINE = core::hal::IRQ_LINE_COUNT;

Capability make(CapabilityType type, uint32_t flags, uint64_t first,
				uint64_t count) {
	Capability capability;
	capability.type = type;
	capability.flags = flags;
	capability.first = first;
	capability.count = count;
	return capability;
}

void checkCapabilitySet() {
	CapabilitySet set;
	EXPECT_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_START, SINGLE));

	EXPECT_TRUE(set.add(make(CapabilityType::IO_PORTS, 0, PORTS_START, PORTS_COUNT)));
	EXPECT_TRUE(set.covers(CapabilityType::IO_PORTS, PORTS_START, PORTS_COUNT));
	EXPECT_TRUE(set.covers(CapabilityType::IO_PORTS, PORTS_START + 1, SINGLE));
	EXPECT_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_START - 1, PORTS_COUNT));
	EXPECT_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_START + 1, PORTS_COUNT));
	EXPECT_FALSE(set.covers(CapabilityType::IRQ, PORTS_START, SINGLE));
	EXPECT_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_START, PAST_THE_END));
	EXPECT_FALSE(set.mayGrant(CapabilityType::IO_PORTS, PORTS_START, SINGLE));

	EXPECT_TRUE(set.add(make(CapabilityType::IRQ, CAPABILITY_FLAG_GRANT,
							 FIRST_LINE, LINE_COUNT)));
	EXPECT_TRUE(set.mayGrant(CapabilityType::IRQ, FIRST_LINE, SINGLE));
	EXPECT_FALSE(set.mayGrant(CapabilityType::IRQ, FIRST_LINE, LINE_COUNT + 1));

	EXPECT_FALSE(set.add(make(CapabilityType::NONE, 0, PORTS_START, SINGLE)));
	EXPECT_FALSE(set.add(make(CapabilityType::IRQ, 0, PORTS_START, 0)));
	EXPECT_FALSE(set.add(make(CapabilityType::IRQ, 0, PAST_THE_END, PORTS_COUNT)));

	set.clear();
	EXPECT_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_START, SINGLE));

	size_t added = 0;
	while (set.add(make(CapabilityType::IRQ, 0, FIRST_LINE, SINGLE)))
		added++;
	EXPECT_EQ(CapabilitySet::MAX_CAPABILITIES, added);
}

void checkIrqRouting() {
	EXPECT_FALSE(IrqRouting::bind(core::hal::TIMER_IRQ_LINE, FIRST_OWNER));
	EXPECT_FALSE(IrqRouting::bind(core::hal::CASCADE_IRQ_LINE, FIRST_OWNER));
	EXPECT_FALSE(IrqRouting::bind(OUT_OF_RANGE_LINE, FIRST_OWNER));

	EXPECT_TRUE(IrqRouting::bind(KEYBOARD_LINE, FIRST_OWNER));
	EXPECT_FALSE(IrqRouting::bind(KEYBOARD_LINE, SECOND_OWNER));
	EXPECT_FALSE(IrqRouting::acknowledge(KEYBOARD_LINE, SECOND_OWNER));
	EXPECT_TRUE(IrqRouting::acknowledge(KEYBOARD_LINE, FIRST_OWNER));

	EXPECT_TRUE(IrqRouting::forward(KEYBOARD_LINE));
	EXPECT_FALSE(IrqRouting::forward(UNBOUND_LINE));

	IrqRouting::release(FIRST_OWNER);
	EXPECT_FALSE(IrqRouting::forward(KEYBOARD_LINE));
	EXPECT_TRUE(IrqRouting::bind(KEYBOARD_LINE, SECOND_OWNER));
	IrqRouting::release(SECOND_OWNER);
}

} // namespace

void test_capabilities() {
	TestRunner::begin_suite("Capabilities and IRQ routing");
	checkCapabilitySet();
	checkIrqRouting();
	TestRunner::end_suite();
}

} // namespace cock::test::security
