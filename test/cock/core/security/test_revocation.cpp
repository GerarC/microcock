#include <cock/core/hal/tasking.hpp>
#include <cock/core/irq/irq_routing.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_tree.hpp>
#include <cock/core/security/revocation.hpp>
#include <cock/core/task/process.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <hal/address_space.hpp>
#include <hal/cpu.hpp>
#include <hal/interrupt.hpp>
#include <hal/page.hpp>
#include <stddef.h>
#include <stdint.h>
#include <test/test_framework.hpp>

namespace cock::test::security {

using core::irq::IrqRouting;
using core::memory::NO_DEVICE_BASE;
using core::memory::NO_SHARED_OBJECT;
using core::memory::PhysAddr;
using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::memory::VMMPermission;
using core::security::capability_tree;
using core::security::Capability;
using core::security::CapabilityNodeId;
using core::security::CapabilityType;
using core::security::CAPABILITY_FLAG_GRANT;
using core::security::NO_CAPABILITY_NODE;
using core::security::revoke_capability;
using core::task::Process;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadPriority;
using core::task::ThreadState;

namespace {

constexpr uintptr_t USER_ENTRY_POINT = 0x400000;
constexpr uint64_t ALL_PORTS = core::hal::IO_PORT_COUNT;
constexpr CapabilityNodeId UNKNOWN_NODE = 0x7000000;
constexpr uint32_t FIRST_PORT = 0x60;
constexpr uint32_t PORT_COUNT = 8;
constexpr uint32_t SECOND_PORT = 0x64;
constexpr uint32_t SECOND_COUNT = 4;
constexpr uint32_t CHILD_PORT = 0x62;
constexpr uint32_t CHILD_COUNT = 2;
constexpr uint32_t SINGLE = 1;
constexpr uint32_t BITS_PER_BYTE = 8;
constexpr uint64_t ALL_LINES = core::hal::IRQ_LINE_COUNT;
constexpr uint32_t KEYBOARD_LINE = 1;
constexpr uint64_t VIDEO_PHYSICAL = 0xB8000;
constexpr uint64_t VIDEO_SPAN = 0x1000;
constexpr uintptr_t DEVICE_VIRTUAL = 0x40000000;
constexpr uint64_t ALL_MEMORY = core::hal::PHYSICAL_ADDRESS_SPACE_SIZE;
constexpr size_t ONE_PAGE = 1;
constexpr size_t NO_REGIONS = 0;

struct Party {
	Process *process = nullptr;
	Thread *thread = nullptr;

	uint32_t id() const { return process->getId(); }
};

Party makeParty() {
	Party party;
	PhysAddr space = VirtualMemoryManager::createAddressSpace();
	party.process =
		Process::create(space, VirtAddr(core::hal::USER_SPACE_LIMIT));
	party.thread = new Thread(*party.process, VirtAddr(USER_ENTRY_POINT),
							  ThreadPriority::NORMAL);
	return party;
}

void retire(Party &party) {
	party.thread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(party.thread);
	TaskManager::buryDeadThreads();
}

CapabilityNodeId giveRoot(Party &party, CapabilityType type, uint64_t first,
						  uint64_t count) {
	Capability capability;
	capability.type = type;
	capability.flags = CAPABILITY_FLAG_GRANT;
	capability.first = first;
	capability.count = count;
	capability.node = capability_tree.createRoot(party.id(), type, first, count);
	party.process->getCapabilities().add(capability);
	return capability.node;
}

CapabilityNodeId give(Party &to, CapabilityNodeId parent, CapabilityType type,
					  uint64_t first, uint64_t count) {
	Capability capability;
	capability.type = type;
	capability.first = first;
	capability.count = count;
	capability.node = capability_tree.derive(parent, to.id(), type, first, count);
	to.process->getCapabilities().add(capability);
	return capability.node;
}

bool portOpen(const Party &party, uint32_t port) {
	const core::hal::ArchProcessContext &context = party.process->getContext();
	if (context.iopm == nullptr) return false;
	return (context.iopm[port / BITS_PER_BYTE] & (1u << (port % BITS_PER_BYTE))) == 0;
}

void openPorts(Party &party, uint32_t first, uint32_t count) {
	EXPECT_TRUE(core::hal::grant_hardware_resource(party.process->getContext(), first, count));
}

bool holds(const Party &party, CapabilityType type, uint64_t first) {
	return party.process->getCapabilities().covers(type, first, SINGLE);
}

void checkPortsAreClosed() {
	Party root = makeParty();
	Party driver = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::IO_PORTS, 0, ALL_PORTS);
	CapabilityNodeId node = give(driver, top, CapabilityType::IO_PORTS, FIRST_PORT, PORT_COUNT);
	openPorts(driver, FIRST_PORT, PORT_COUNT);
	EXPECT_TRUE(portOpen(driver, FIRST_PORT));
	EXPECT_TRUE(portOpen(driver, FIRST_PORT + PORT_COUNT - SINGLE));

	EXPECT_TRUE(revoke_capability(*root.process, node));
	EXPECT_FALSE(portOpen(driver, FIRST_PORT));
	EXPECT_FALSE(portOpen(driver, FIRST_PORT + PORT_COUNT - SINGLE));
	EXPECT_FALSE(holds(driver, CapabilityType::IO_PORTS, FIRST_PORT));
	EXPECT_TRUE(capability_tree.find(node) == nullptr);
	EXPECT_TRUE(holds(root, CapabilityType::IO_PORTS, FIRST_PORT));
	EXPECT_FALSE(revoke_capability(*root.process, node));

	retire(driver);
	retire(root);
}

void checkRefusals() {
	Party root = makeParty();
	Party first = makeParty();
	Party second = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::IO_PORTS, 0, ALL_PORTS);
	CapabilityNodeId mine = give(first, top, CapabilityType::IO_PORTS, FIRST_PORT, PORT_COUNT);
	CapabilityNodeId theirs = give(second, top, CapabilityType::IO_PORTS, SECOND_PORT, SECOND_COUNT);

	EXPECT_FALSE(revoke_capability(*first.process, mine));
	EXPECT_FALSE(revoke_capability(*first.process, theirs));
	EXPECT_FALSE(revoke_capability(*first.process, top));
	EXPECT_FALSE(revoke_capability(*root.process, top));
	EXPECT_FALSE(revoke_capability(*root.process, NO_CAPABILITY_NODE));
	EXPECT_FALSE(revoke_capability(*root.process, UNKNOWN_NODE));
	EXPECT_TRUE(holds(first, CapabilityType::IO_PORTS, FIRST_PORT));
	EXPECT_TRUE(holds(second, CapabilityType::IO_PORTS, SECOND_PORT));
	EXPECT_TRUE(capability_tree.find(mine) != nullptr);

	retire(second);
	retire(first);
	retire(root);
}

void checkOverlappingNodesKeepWhatIsStillCovered() {
	Party root = makeParty();
	Party driver = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::IO_PORTS, 0, ALL_PORTS);
	CapabilityNodeId wide = give(driver, top, CapabilityType::IO_PORTS, FIRST_PORT, PORT_COUNT);
	CapabilityNodeId narrow = give(driver, top, CapabilityType::IO_PORTS, SECOND_PORT, SECOND_COUNT);
	openPorts(driver, FIRST_PORT, PORT_COUNT);

	EXPECT_TRUE(revoke_capability(*root.process, wide));
	EXPECT_FALSE(portOpen(driver, FIRST_PORT));
	EXPECT_FALSE(portOpen(driver, SECOND_PORT - SINGLE));
	EXPECT_TRUE(portOpen(driver, SECOND_PORT));
	EXPECT_TRUE(portOpen(driver, SECOND_PORT + SECOND_COUNT - SINGLE));
	EXPECT_FALSE(portOpen(driver, SECOND_PORT + SECOND_COUNT));

	EXPECT_TRUE(revoke_capability(*root.process, narrow));
	EXPECT_FALSE(portOpen(driver, SECOND_PORT));

	retire(driver);
	retire(root);
}

void checkSubtreeIsRevoked() {
	Party root = makeParty();
	Party driver = makeParty();
	Party client = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::IO_PORTS, 0, ALL_PORTS);
	CapabilityNodeId node = give(driver, top, CapabilityType::IO_PORTS, FIRST_PORT, PORT_COUNT);
	CapabilityNodeId child = give(client, node, CapabilityType::IO_PORTS, CHILD_PORT, CHILD_COUNT);
	openPorts(driver, FIRST_PORT, PORT_COUNT);
	openPorts(client, CHILD_PORT, CHILD_COUNT);

	EXPECT_TRUE(revoke_capability(*root.process, node));
	EXPECT_FALSE(portOpen(driver, FIRST_PORT));
	EXPECT_FALSE(portOpen(client, CHILD_PORT));
	EXPECT_FALSE(holds(client, CapabilityType::IO_PORTS, CHILD_PORT));
	EXPECT_TRUE(capability_tree.find(child) == nullptr);

	retire(client);
	retire(driver);
	retire(root);
}

void checkMiddleHolderRevokesBelow() {
	Party root = makeParty();
	Party driver = makeParty();
	Party client = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::IO_PORTS, 0, ALL_PORTS);
	CapabilityNodeId node = give(driver, top, CapabilityType::IO_PORTS, FIRST_PORT, PORT_COUNT);
	CapabilityNodeId child = give(client, node, CapabilityType::IO_PORTS, CHILD_PORT, CHILD_COUNT);
	openPorts(driver, FIRST_PORT, PORT_COUNT);
	openPorts(client, CHILD_PORT, CHILD_COUNT);

	EXPECT_FALSE(revoke_capability(*client.process, child));
	EXPECT_TRUE(revoke_capability(*driver.process, child));
	EXPECT_FALSE(portOpen(client, CHILD_PORT));
	EXPECT_TRUE(portOpen(driver, CHILD_PORT));
	EXPECT_TRUE(holds(driver, CapabilityType::IO_PORTS, FIRST_PORT));

	retire(client);
	retire(driver);
	retire(root);
}

void checkInterruptLineIsUnbound() {
	Party root = makeParty();
	Party driver = makeParty();
	Party other = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::IRQ, 0, ALL_LINES);
	CapabilityNodeId node = give(driver, top, CapabilityType::IRQ, KEYBOARD_LINE, SINGLE);
	EXPECT_TRUE(IrqRouting::bind(KEYBOARD_LINE, driver.thread->getId()));
	EXPECT_FALSE(IrqRouting::bind(KEYBOARD_LINE, other.thread->getId()));

	EXPECT_TRUE(revoke_capability(*root.process, node));
	uint32_t owner = 0;
	EXPECT_FALSE(IrqRouting::ownerOf(KEYBOARD_LINE, owner));
	EXPECT_FALSE(holds(driver, CapabilityType::IRQ, KEYBOARD_LINE));
	EXPECT_TRUE(IrqRouting::bind(KEYBOARD_LINE, other.thread->getId()));
	IrqRouting::release(other.thread->getId());

	retire(other);
	retire(driver);
	retire(root);
}

void checkOtherOwnerKeepsItsLine() {
	Party root = makeParty();
	Party driver = makeParty();
	Party other = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::IRQ, 0, ALL_LINES);
	CapabilityNodeId node = give(driver, top, CapabilityType::IRQ, KEYBOARD_LINE, SINGLE);
	EXPECT_TRUE(IrqRouting::bind(KEYBOARD_LINE, other.thread->getId()));

	EXPECT_TRUE(revoke_capability(*root.process, node));
	uint32_t owner = 0;
	EXPECT_TRUE(IrqRouting::ownerOf(KEYBOARD_LINE, owner));
	EXPECT_EQ(other.thread->getId(), owner);
	IrqRouting::release(other.thread->getId());

	retire(other);
	retire(driver);
	retire(root);
}

bool deviceMapped(const Party &party) {
	PhysAddr previous = core::hal::get_current_address_space();
	core::hal::switch_address_space(party.process->getAddressSpace());
	bool mapped = !VirtualMemoryManager::isRangeUnmapped(VirtAddr(DEVICE_VIRTUAL), ONE_PAGE);
	core::hal::switch_address_space(previous);
	return mapped;
}

void mapDevice(Party &party) {
	PhysAddr previous = core::hal::get_current_address_space();
	core::hal::switch_address_space(party.process->getAddressSpace());
	EXPECT_TRUE(VirtualMemoryManager::map(PhysAddr(VIDEO_PHYSICAL), VirtAddr(DEVICE_VIRTUAL),
										  VMMPermission::USER_DATA | VMMPermission::DEVICE));
	core::hal::switch_address_space(previous);
	EXPECT_TRUE(party.process->addMemoryRegion(VirtAddr(DEVICE_VIRTUAL), ONE_PAGE, false,
											   NO_SHARED_OBJECT, VIDEO_PHYSICAL));
}

void checkDeviceMappingIsRemoved() {
	Party root = makeParty();
	Party driver = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::PHYSICAL_MEMORY, 0, ALL_MEMORY);
	CapabilityNodeId node = give(driver, top, CapabilityType::PHYSICAL_MEMORY, VIDEO_PHYSICAL, VIDEO_SPAN);
	size_t regions = driver.process->getRegionCount();
	mapDevice(driver);
	EXPECT_TRUE(deviceMapped(driver));
	EXPECT_EQ(regions + ONE_PAGE, driver.process->getRegionCount());

	EXPECT_TRUE(revoke_capability(*root.process, node));
	EXPECT_FALSE(deviceMapped(driver));
	EXPECT_EQ(regions, driver.process->getRegionCount());
	EXPECT_FALSE(holds(driver, CapabilityType::PHYSICAL_MEMORY, VIDEO_PHYSICAL));

	retire(driver);
	retire(root);
}

void checkCoveredDeviceMappingStays() {
	Party root = makeParty();
	Party driver = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::PHYSICAL_MEMORY, 0, ALL_MEMORY);
	CapabilityNodeId first = give(driver, top, CapabilityType::PHYSICAL_MEMORY, VIDEO_PHYSICAL, VIDEO_SPAN);
	CapabilityNodeId second = give(driver, top, CapabilityType::PHYSICAL_MEMORY, VIDEO_PHYSICAL, VIDEO_SPAN);
	mapDevice(driver);

	EXPECT_TRUE(revoke_capability(*root.process, first));
	EXPECT_TRUE(deviceMapped(driver));
	EXPECT_TRUE(revoke_capability(*root.process, second));
	EXPECT_FALSE(deviceMapped(driver));

	retire(driver);
	retire(root);
}

void checkHolderThatExitedLeavesNothing() {
	Party root = makeParty();
	Party driver = makeParty();
	CapabilityNodeId top = giveRoot(root, CapabilityType::IO_PORTS, 0, ALL_PORTS);
	CapabilityNodeId node = give(driver, top, CapabilityType::IO_PORTS, FIRST_PORT, PORT_COUNT);
	size_t before = capability_tree.size();

	retire(driver);
	EXPECT_EQ(before - SINGLE, capability_tree.size());
	EXPECT_TRUE(capability_tree.find(node) == nullptr);
	EXPECT_FALSE(revoke_capability(*root.process, node));

	retire(root);
}

} // namespace

void test_revocation() {
	TestRunner::begin_suite("Capability revocation");
	size_t before = capability_tree.size();
	checkPortsAreClosed();
	checkRefusals();
	checkOverlappingNodesKeepWhatIsStillCovered();
	checkSubtreeIsRevoked();
	checkMiddleHolderRevokesBelow();
	checkInterruptLineIsUnbound();
	checkOtherOwnerKeepsItsLine();
	checkDeviceMappingIsRemoved();
	checkCoveredDeviceMappingStays();
	checkHolderThatExitedLeavesNothing();
	EXPECT_EQ(before, capability_tree.size());
	TestRunner::end_suite();
}

} // namespace cock::test::security
