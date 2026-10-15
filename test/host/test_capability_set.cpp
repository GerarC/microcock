#include <cock/core/security/capability.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>

namespace host_test {

using cock::core::security::Capability;
using cock::core::security::CAPABILITY_FLAG_GRANT;
using cock::core::security::CapabilitySet;
using cock::core::security::CapabilityType;

namespace {

constexpr uint64_t PORTS_FIRST = 0x60;
constexpr uint64_t PORTS_COUNT = 4;
constexpr uint64_t OTHER_FIRST = 0x3F8;
constexpr uint64_t SINGLE = 1;
constexpr uint64_t NOTHING = 0;
constexpr uint64_t LAST_VALUE = UINT64_MAX;
constexpr uint32_t NO_FLAGS = 0;
constexpr uint64_t IRQ_FIRST = 1;
constexpr uint64_t IRQ_COUNT = 2;
constexpr uint64_t DOUBLE = 2;

Capability make(CapabilityType type, uint32_t flags, uint64_t first,
				uint64_t count) {
	Capability capability;
	capability.type = type;
	capability.flags = flags;
	capability.first = first;
	capability.count = count;
	return capability;
}

Capability ports(uint32_t flags, uint64_t first, uint64_t count) {
	return make(CapabilityType::IO_PORTS, flags, first, count);
}

void checkRejectedEntries() {
	CapabilitySet set;
	CHECK_FALSE(set.add(make(CapabilityType::NONE, NO_FLAGS, PORTS_FIRST, PORTS_COUNT)));
	CHECK_FALSE(set.add(ports(NO_FLAGS, PORTS_FIRST, NOTHING)));
	CHECK_FALSE(set.add(ports(NO_FLAGS, LAST_VALUE, PORTS_COUNT)));
	CHECK_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST, SINGLE));
}

void checkCoverage() {
	CapabilitySet set;
	CHECK_TRUE(set.add(ports(NO_FLAGS, PORTS_FIRST, PORTS_COUNT)));

	CHECK_TRUE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST, PORTS_COUNT));
	CHECK_TRUE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST, SINGLE));
	CHECK_TRUE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST + PORTS_COUNT - SINGLE, SINGLE));
	CHECK_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST + PORTS_COUNT, SINGLE));
	CHECK_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST - SINGLE, SINGLE));
	CHECK_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST, PORTS_COUNT + SINGLE));
	CHECK_FALSE(set.covers(CapabilityType::IRQ, PORTS_FIRST, SINGLE));
	CHECK_FALSE(set.covers(CapabilityType::NONE, PORTS_FIRST, SINGLE));
	CHECK_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST, NOTHING));
	CHECK_FALSE(set.covers(CapabilityType::IO_PORTS, LAST_VALUE, PORTS_COUNT));
}

void checkRangesAreNotMerged() {
	CapabilitySet set;
	CHECK_TRUE(set.add(ports(NO_FLAGS, PORTS_FIRST, PORTS_COUNT)));
	CHECK_TRUE(set.add(ports(NO_FLAGS, PORTS_FIRST + PORTS_COUNT, PORTS_COUNT)));

	CHECK_TRUE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST, PORTS_COUNT));
	CHECK_TRUE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST + PORTS_COUNT, PORTS_COUNT));
	CHECK_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST, PORTS_COUNT * DOUBLE));
}

void checkGrantAuthority() {
	CapabilitySet set;
	CHECK_FALSE(set.holdsGrantAuthority());

	CHECK_TRUE(set.add(ports(NO_FLAGS, PORTS_FIRST, PORTS_COUNT)));
	CHECK_FALSE(set.holdsGrantAuthority());
	CHECK_FALSE(set.mayGrant(CapabilityType::IO_PORTS, PORTS_FIRST, SINGLE));

	CHECK_TRUE(set.add(make(CapabilityType::IRQ, CAPABILITY_FLAG_GRANT, IRQ_FIRST, IRQ_COUNT)));
	CHECK_TRUE(set.holdsGrantAuthority());
	CHECK_TRUE(set.mayGrant(CapabilityType::IRQ, IRQ_FIRST, IRQ_COUNT));
	CHECK_FALSE(set.mayGrant(CapabilityType::IRQ, IRQ_FIRST, IRQ_COUNT + SINGLE));
	CHECK_FALSE(set.mayGrant(CapabilityType::IO_PORTS, PORTS_FIRST, SINGLE));
	CHECK_FALSE(set.mayGrant(CapabilityType::IRQ, IRQ_FIRST, NOTHING));
}

void checkTableIsBounded() {
	CapabilitySet set;
	for (size_t idx = 0; idx < CapabilitySet::MAX_CAPABILITIES; idx++)
		CHECK_TRUE(set.add(ports(NO_FLAGS, PORTS_FIRST + idx, SINGLE)));
	CHECK_FALSE(set.add(ports(NO_FLAGS, OTHER_FIRST, SINGLE)));
	CHECK_FALSE(set.covers(CapabilityType::IO_PORTS, OTHER_FIRST, SINGLE));
}

void checkClear() {
	CapabilitySet set;
	CHECK_TRUE(set.add(ports(CAPABILITY_FLAG_GRANT, PORTS_FIRST, PORTS_COUNT)));
	set.clear();
	CHECK_FALSE(set.covers(CapabilityType::IO_PORTS, PORTS_FIRST, SINGLE));
	CHECK_FALSE(set.holdsGrantAuthority());

	for (size_t idx = 0; idx < CapabilitySet::MAX_CAPABILITIES; idx++)
		CHECK_TRUE(set.add(make(CapabilityType::IRQ, NO_FLAGS, IRQ_FIRST + idx, SINGLE)));
}

} // namespace

void testCapabilitySet() {
	checkRejectedEntries();
	checkCoverage();
	checkRangesAreNotMerged();
	checkGrantAuthority();
	checkTableIsBounded();
	checkClear();
}

} // namespace host_test
