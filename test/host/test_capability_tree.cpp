#include <cock/core/security/capability_tree.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>

namespace host_test {

using cock::core::security::CapabilityNode;
using cock::core::security::CapabilityNodeId;
using cock::core::security::CapabilityTree;
using cock::core::security::CapabilityType;
using cock::core::security::FIRST_CAPABILITY_NODE;
using cock::core::security::HIGHEST_CAPABILITY_NODE;
using cock::core::security::NO_CAPABILITY_NODE;

namespace {

constexpr uint32_t ROOT_HOLDER = 1;
constexpr uint32_t DRIVER_HOLDER = 5;
constexpr uint32_t CLIENT_HOLDER = 6;
constexpr uint64_t PORTS_FIRST = 0;
constexpr uint64_t PORTS_COUNT = 0x10000;
constexpr uint64_t DRIVER_FIRST = 0x60;
constexpr uint64_t DRIVER_COUNT = 8;
constexpr uint64_t CLIENT_FIRST = 0x62;
constexpr uint64_t CLIENT_COUNT = 2;
constexpr uint64_t SINGLE = 1;
constexpr uint64_t NOTHING = 0;
constexpr uint64_t LAST_VALUE = UINT64_MAX;
constexpr CapabilityNodeId UNKNOWN_NODE = 1000;
constexpr size_t NO_NODES = 0;
constexpr size_t ONE_NODE = 1;
constexpr size_t TWO_NODES = 2;
constexpr size_t THREE_NODES = 3;

CapabilityNodeId rootPorts(CapabilityTree &tree) {
	return tree.createRoot(ROOT_HOLDER, CapabilityType::IO_PORTS, PORTS_FIRST, PORTS_COUNT);
}

void checkRoot() {
	CapabilityTree tree;
	CHECK_EQ(NO_NODES, tree.size());

	CapabilityNodeId root = rootPorts(tree);
	CHECK_EQ(FIRST_CAPABILITY_NODE, root);
	CHECK_EQ(ONE_NODE, tree.size());

	const CapabilityNode *node = tree.find(root);
	CHECK_TRUE(node != nullptr);
	if (node == nullptr) return;
	CHECK_EQ(NO_CAPABILITY_NODE, node->parent);
	CHECK_EQ(ROOT_HOLDER, node->holder);
	CHECK_EQ(PORTS_COUNT, node->count);

	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.createRoot(ROOT_HOLDER, CapabilityType::NONE, PORTS_FIRST, PORTS_COUNT));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.createRoot(ROOT_HOLDER, CapabilityType::IRQ, PORTS_FIRST, NOTHING));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.createRoot(ROOT_HOLDER, CapabilityType::IRQ, LAST_VALUE, PORTS_COUNT));
	CHECK_TRUE(tree.find(NO_CAPABILITY_NODE) == nullptr);
}

void checkDerivation() {
	CapabilityTree tree;
	CapabilityNodeId root = rootPorts(tree);

	CapabilityNodeId driver = tree.derive(root, DRIVER_HOLDER, CapabilityType::IO_PORTS,
										  DRIVER_FIRST, DRIVER_COUNT);
	CHECK_TRUE(driver != NO_CAPABILITY_NODE);
	CapabilityNodeId client = tree.derive(driver, CLIENT_HOLDER, CapabilityType::IO_PORTS,
										  CLIENT_FIRST, CLIENT_COUNT);
	CHECK_TRUE(client != NO_CAPABILITY_NODE);
	CHECK_EQ(THREE_NODES, tree.size());

	const CapabilityNode *node = tree.find(client);
	CHECK_TRUE(node != nullptr);
	if (node == nullptr) return;
	CHECK_EQ(driver, node->parent);
	CHECK_EQ(CLIENT_HOLDER, node->holder);

	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.derive(driver, CLIENT_HOLDER, CapabilityType::IO_PORTS,
						 DRIVER_FIRST + DRIVER_COUNT, SINGLE));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.derive(driver, CLIENT_HOLDER, CapabilityType::IO_PORTS,
						 DRIVER_FIRST - SINGLE, SINGLE));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.derive(driver, CLIENT_HOLDER, CapabilityType::IO_PORTS,
						 DRIVER_FIRST, DRIVER_COUNT + SINGLE));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.derive(driver, CLIENT_HOLDER, CapabilityType::IRQ, DRIVER_FIRST, SINGLE));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.derive(driver, CLIENT_HOLDER, CapabilityType::IO_PORTS, DRIVER_FIRST, NOTHING));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.derive(driver, CLIENT_HOLDER, CapabilityType::IO_PORTS, LAST_VALUE, SINGLE));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.derive(NO_CAPABILITY_NODE, CLIENT_HOLDER, CapabilityType::IO_PORTS,
						 DRIVER_FIRST, SINGLE));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.derive(UNKNOWN_NODE, CLIENT_HOLDER, CapabilityType::IO_PORTS,
						 DRIVER_FIRST, SINGLE));
	CHECK_EQ(THREE_NODES, tree.size());
}

void checkDetachMovesChildrenUp() {
	CapabilityTree tree;
	CapabilityNodeId root = rootPorts(tree);
	CapabilityNodeId driver = tree.derive(root, DRIVER_HOLDER, CapabilityType::IO_PORTS,
										  DRIVER_FIRST, DRIVER_COUNT);
	CapabilityNodeId client = tree.derive(driver, CLIENT_HOLDER, CapabilityType::IO_PORTS,
										  CLIENT_FIRST, CLIENT_COUNT);

	CHECK_TRUE(tree.detach(driver));
	CHECK_FALSE(tree.detach(driver));
	CHECK_TRUE(tree.find(driver) == nullptr);
	CHECK_EQ(TWO_NODES, tree.size());

	const CapabilityNode *node = tree.find(client);
	CHECK_TRUE(node != nullptr);
	if (node == nullptr) return;
	CHECK_EQ(root, node->parent);

	CHECK_TRUE(tree.detach(root));
	node = tree.find(client);
	CHECK_TRUE(node != nullptr);
	if (node == nullptr) return;
	CHECK_EQ(NO_CAPABILITY_NODE, node->parent);
}

void checkReleaseHolder() {
	CapabilityTree tree;
	CapabilityNodeId root = rootPorts(tree);
	CapabilityNodeId driver = tree.derive(root, DRIVER_HOLDER, CapabilityType::IO_PORTS,
										  DRIVER_FIRST, DRIVER_COUNT);
	CapabilityNodeId second = tree.derive(root, DRIVER_HOLDER, CapabilityType::IO_PORTS,
										  PORTS_FIRST, SINGLE);
	CapabilityNodeId client = tree.derive(driver, CLIENT_HOLDER, CapabilityType::IO_PORTS,
										  CLIENT_FIRST, CLIENT_COUNT);

	CHECK_EQ(TWO_NODES, tree.releaseHolder(DRIVER_HOLDER));
	CHECK_TRUE(tree.find(driver) == nullptr);
	CHECK_TRUE(tree.find(second) == nullptr);
	CHECK_EQ(NO_NODES, tree.releaseHolder(DRIVER_HOLDER));

	const CapabilityNode *node = tree.find(client);
	CHECK_TRUE(node != nullptr);
	if (node == nullptr) return;
	CHECK_EQ(root, node->parent);
	CHECK_EQ(TWO_NODES, tree.size());
}

void checkTableIsBounded() {
	CapabilityTree tree;
	CapabilityNodeId root = rootPorts(tree);
	for (size_t idx = ONE_NODE; idx < CapabilityTree::MAX_NODES; idx++)
		CHECK_TRUE(tree.derive(root, DRIVER_HOLDER, CapabilityType::IO_PORTS, idx, SINGLE) !=
				   NO_CAPABILITY_NODE);
	CHECK_EQ(CapabilityTree::MAX_NODES, tree.size());
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.derive(root, DRIVER_HOLDER, CapabilityType::IO_PORTS, PORTS_FIRST, SINGLE));
	CHECK_EQ(NO_CAPABILITY_NODE,
			 tree.createRoot(ROOT_HOLDER, CapabilityType::IRQ, PORTS_FIRST, SINGLE));

	CHECK_EQ(CapabilityTree::MAX_NODES - ONE_NODE, tree.releaseHolder(DRIVER_HOLDER));
	CHECK_TRUE(tree.derive(root, DRIVER_HOLDER, CapabilityType::IO_PORTS, PORTS_FIRST, SINGLE) !=
			   NO_CAPABILITY_NODE);
}

void checkIdsAreNotReused() {
	CapabilityTree tree;
	CapabilityNodeId root = rootPorts(tree);
	CapabilityNodeId first = tree.derive(root, DRIVER_HOLDER, CapabilityType::IO_PORTS,
										 DRIVER_FIRST, SINGLE);
	CHECK_TRUE(tree.detach(first));
	CapabilityNodeId second = tree.derive(root, DRIVER_HOLDER, CapabilityType::IO_PORTS,
										  DRIVER_FIRST, SINGLE);
	CHECK_TRUE(second != first);
	CHECK_TRUE(second != NO_CAPABILITY_NODE);
	CHECK_TRUE(second <= HIGHEST_CAPABILITY_NODE);
}

void checkReset() {
	CapabilityTree tree;
	rootPorts(tree);
	tree.reset();
	CHECK_EQ(NO_NODES, tree.size());
	CHECK_EQ(FIRST_CAPABILITY_NODE, rootPorts(tree));
}

} // namespace

void testCapabilityTree() {
	checkRoot();
	checkDerivation();
	checkDetachMovesChildrenUp();
	checkReleaseHolder();
	checkTableIsBounded();
	checkIdsAreNotReused();
	checkReset();
}

} // namespace host_test
