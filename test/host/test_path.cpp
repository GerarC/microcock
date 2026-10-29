#include <cock/path/resolve.hpp>
#include <string.h>
#include <test/host/check.hpp>

namespace host_test {

namespace {

constexpr size_t BUFFER_BYTES = 32;
constexpr size_t TIGHT_BYTES = 6;
constexpr size_t EXACT_BYTES = 9;
constexpr char ROOT[] = "/";
constexpr char DIRECTORY[] = "/usr/lib";

bool resolvesTo(const char *base, const char *path, const char *expected) {
	char out[BUFFER_BYTES];
	return cock::path::resolve(base, path, out, sizeof(out)) && strcmp(out, expected) == 0;
}

void checkAbsolute() {
	CHECK_TRUE(resolvesTo(DIRECTORY, "/fat/a.txt", "/fat/a.txt"));
	CHECK_TRUE(resolvesTo(DIRECTORY, "/", ROOT));
	CHECK_TRUE(resolvesTo(DIRECTORY, "//fat///a/", "/fat/a"));
	CHECK_TRUE(resolvesTo(nullptr, "/fat", "/fat"));
}

void checkRelative() {
	CHECK_TRUE(resolvesTo(DIRECTORY, "bin", "/usr/lib/bin"));
	CHECK_TRUE(resolvesTo(DIRECTORY, "./bin/./x", "/usr/lib/bin/x"));
	CHECK_TRUE(resolvesTo(DIRECTORY, ".", DIRECTORY));
	CHECK_TRUE(resolvesTo(DIRECTORY, "", DIRECTORY));
	CHECK_TRUE(resolvesTo(ROOT, "fat", "/fat"));
	CHECK_TRUE(resolvesTo(nullptr, "fat", "/fat"));
}

void checkParent() {
	CHECK_TRUE(resolvesTo(DIRECTORY, "..", "/usr"));
	CHECK_TRUE(resolvesTo(DIRECTORY, "../..", ROOT));
	CHECK_TRUE(resolvesTo(DIRECTORY, "../../..", ROOT));
	CHECK_TRUE(resolvesTo(DIRECTORY, "../bin", "/usr/bin"));
	CHECK_TRUE(resolvesTo(DIRECTORY, "a/../b", "/usr/lib/b"));
	CHECK_TRUE(resolvesTo(DIRECTORY, "/fat/../ext", "/ext"));
	CHECK_TRUE(resolvesTo(ROOT, "..", ROOT));
	CHECK_TRUE(resolvesTo("/a/../b/.", "c", "/b/c"));
	CHECK_TRUE(resolvesTo(DIRECTORY, "...", "/usr/lib/..."));
	CHECK_TRUE(resolvesTo(DIRECTORY, ".a", "/usr/lib/.a"));
}

void checkLimits() {
	char out[BUFFER_BYTES];
	CHECK_FALSE(cock::path::resolve(DIRECTORY, nullptr, out, sizeof(out)));
	CHECK_TRUE(out[0] == cock::path::TERMINATOR);
	CHECK_FALSE(cock::path::resolve(DIRECTORY, "a", nullptr, sizeof(out)));
	CHECK_FALSE(cock::path::resolve(DIRECTORY, "a", out, 0));
	CHECK_FALSE(cock::path::resolve(DIRECTORY, "a", out, cock::path::ROOT_LENGTH));
	CHECK_TRUE(cock::path::resolve(DIRECTORY, "/", out, cock::path::MIN_CAPACITY));

	CHECK_FALSE(cock::path::resolve(ROOT, "abcde", out, TIGHT_BYTES));
	CHECK_TRUE(out[0] == cock::path::TERMINATOR);
	CHECK_TRUE(cock::path::resolve(ROOT, "abcd", out, TIGHT_BYTES));
	CHECK_TRUE(strcmp(out, "/abcd") == 0);
	CHECK_FALSE(cock::path::resolve("/abcd", "ef", out, EXACT_BYTES - 2));
	CHECK_TRUE(cock::path::resolve("/abcd", "ef", out, EXACT_BYTES));
	CHECK_TRUE(strcmp(out, "/abcd/ef") == 0);
	CHECK_FALSE(cock::path::resolve("/abcd", "efg", out, EXACT_BYTES));
}

} // namespace

void testPath() {
	checkAbsolute();
	checkRelative();
	checkParent();
	checkLimits();
}

} // namespace host_test
