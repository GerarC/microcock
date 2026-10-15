#include <cock/abi/fs.hpp>
#include <host_test/check.hpp>
#include <stddef.h>
#include <string.h>

namespace host_test {

namespace {

constexpr size_t PARENT_CAPACITY = 16;
constexpr size_t TINY_CAPACITY = 3;

bool splits(const char *path, const char *parent, const char *leaf) {
	char found[PARENT_CAPACITY] = {};
	const char *name = nullptr;
	size_t length = 0;
	return cock::abi::fs::splitPath(path, found, sizeof(found), name, length) &&
		   strcmp(found, parent) == 0 && length == strlen(leaf) &&
		   memcmp(name, leaf, length) == 0;
}

void checkSplitPath() {
	CHECK_TRUE(splits("/a/b/c", "/a/b", "c"));
	CHECK_TRUE(splits("/a", "/", "a"));
	CHECK_TRUE(splits("a", "/", "a"));
	CHECK_TRUE(splits("a/b", "a", "b"));
	CHECK_TRUE(splits("//a//b//", "//a", "b"));
	CHECK_TRUE(splits("/a/b/", "/a", "b"));

	char parent[PARENT_CAPACITY];
	const char *leaf = nullptr;
	size_t length = 0;
	CHECK_FALSE(cock::abi::fs::splitPath("/", parent, sizeof(parent), leaf, length));
	CHECK_FALSE(cock::abi::fs::splitPath("", parent, sizeof(parent), leaf, length));
	CHECK_FALSE(cock::abi::fs::splitPath("///", parent, sizeof(parent), leaf, length));
	CHECK_FALSE(cock::abi::fs::splitPath("/abc/d", parent, TINY_CAPACITY, leaf, length));
}

void checkOpenFlags() {
	using namespace cock::abi::fs;
	CHECK_EQ(OPEN_WRITE | OPEN_CREATE | OPEN_TRUNCATE, OPEN_FLAGS_MASK);
	CHECK_EQ(0u, OPEN_READ_ONLY & OPEN_FLAGS_MASK);
	CHECK_EQ(0u, OPEN_WRITE & OPEN_CREATE);
	CHECK_EQ(0u, OPEN_CREATE & OPEN_TRUNCATE);
	CHECK_EQ(0u, OPEN_WRITE & OPEN_TRUNCATE);
}

} // namespace

void testFs() {
	checkSplitPath();
	checkOpenFlags();
}

} // namespace host_test
