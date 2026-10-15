#include <host_test/check.hpp>
#include <stdio.h>

namespace host_test {
void testRingBuffer();
void testIntrusiveQueue();
void testLinkedList();
void testMessage();
void testCapabilitySet();
void testManifest();
void testPartition();
void testFat32();
void testMounts();
void testExt2();
} // namespace host_test

int main() {
	host_test::testRingBuffer();
	host_test::testIntrusiveQueue();
	host_test::testLinkedList();
	host_test::testMessage();
	host_test::testCapabilitySet();
	host_test::testManifest();
	host_test::testPartition();
	host_test::testFat32();
	host_test::testMounts();
	host_test::testExt2();

	printf("host tests: %d checks, %d failed\n", host_test::checksRun,
		   host_test::checksFailed);
	return host_test::checksFailed == 0 ? 0 : 1;
}
