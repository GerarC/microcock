#include <test/host/check.hpp>
#include <stdio.h>

namespace host_test {
void testRingBuffer();
void testIntrusiveQueue();
void testLinkedList();
void testMessage();
void testCapabilitySet();
void testCapabilityTree();
void testManifest();
void testPartition();
void testFat32();
void testMounts();
void testExt2();
void testFs();
void testDatetime();
void testRangePool();
void testConsole();
void testPrng();
void testShell();
void testElfImage();
void testNames();
void testEndpointTable();
void testReplyTable();
void testPmTable();
void testExecArgs();
void testElfMutator();
} // namespace host_test

int main() {
	host_test::testRingBuffer();
	host_test::testIntrusiveQueue();
	host_test::testLinkedList();
	host_test::testMessage();
	host_test::testCapabilitySet();
	host_test::testCapabilityTree();
	host_test::testManifest();
	host_test::testPartition();
	host_test::testFat32();
	host_test::testMounts();
	host_test::testExt2();
	host_test::testFs();
	host_test::testDatetime();
	host_test::testRangePool();
	host_test::testConsole();
	host_test::testPrng();
	host_test::testShell();
	host_test::testElfImage();
	host_test::testNames();
	host_test::testEndpointTable();
	host_test::testReplyTable();
	host_test::testPmTable();
	host_test::testExecArgs();
	host_test::testElfMutator();

	printf("host tests: %d checks, %d failed\n", host_test::checksRun,
		   host_test::checksFailed);
	return host_test::checksFailed == 0 ? 0 : 1;
}
