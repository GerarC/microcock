#include <cock/abi/names.hpp>
#include <names/policy.hpp>
#include <names/services.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using namespace cock::abi::names;
using cock::names::allowTalk;
using cock::names::defineService;
using cock::names::findByLabel;
using cock::names::findByName;
using cock::names::findByPid;
using cock::names::lists;
using cock::names::mayConnect;
using cock::names::NOT_RUNNING;
using cock::names::Service;

namespace {

constexpr char SHORT_NAME[] = "console";
constexpr char TWELVE_BYTES[] = "abcdefghijkl";
constexpr char THIRTEEN_BYTES[] = "abcdefghijklm";
constexpr char EMPTY_NAME[] = "";
constexpr uint32_t SOME_LABEL = 0xABCDEF;
constexpr uint32_t LABEL_TOO_BIG = MAX_LABEL + 1;
constexpr uint32_t CONSOLE_LABEL = 16;
constexpr uint32_t VFS_LABEL = 17;
constexpr uint32_t SHELL_LABEL = 18;
constexpr uint32_t OUTSIDER_LABEL = 19;
constexpr uint32_t UNKNOWN_LABEL = 99;
constexpr uint32_t SHELL_PID = 7;
constexpr uint32_t OTHER_PID = 8;
constexpr size_t NO_ENTRIES = 0;
constexpr size_t ONE_ENTRY = 1;

void checkHeader() {
	uint32_t header = pack(Op::DEFINE, SOME_LABEL);
	CHECK_EQ(static_cast<uint32_t>(Op::DEFINE), opOf(header));
	CHECK_EQ(SOME_LABEL, labelOf(header));
	CHECK_EQ(static_cast<uint32_t>(Op::CONNECT), opOf(pack(Op::CONNECT)));
	CHECK_EQ(NO_ENTRIES, static_cast<size_t>(labelOf(pack(Op::CONNECT))));
	CHECK_EQ(LABEL_TOO_BIG & MAX_LABEL, labelOf(pack(Op::BIND, LABEL_TOO_BIG)));
}

void checkNames() {
	uint32_t words[CALL_WORDS] = {pack(Op::CONNECT)};
	char name[MAX_NAME_BYTES + 1] = {};

	CHECK_TRUE(packName(SHORT_NAME, words));
	CHECK_EQ(strlen(SHORT_NAME), unpackName(words, name));
	CHECK_TRUE(strcmp(name, SHORT_NAME) == 0);
	CHECK_EQ(static_cast<uint32_t>(Op::CONNECT), opOf(words[HEADER_WORD]));

	CHECK_TRUE(packName(TWELVE_BYTES, words));
	CHECK_EQ(MAX_NAME_BYTES, unpackName(words, name));
	CHECK_TRUE(strcmp(name, TWELVE_BYTES) == 0);

	CHECK_FALSE(packName(THIRTEEN_BYTES, words));
	CHECK_EQ(NO_ENTRIES, unpackName(words, name));
	CHECK_EQ(NO_ENTRIES, strlen(name));

	CHECK_FALSE(packName(EMPTY_NAME, words));
	CHECK_EQ(NO_ENTRIES, unpackName(words, name));
}

void checkServices() {
	Service *console = defineService(CONSOLE_LABEL, SHORT_NAME);
	CHECK_TRUE(console != nullptr);
	CHECK_TRUE(defineService(CONSOLE_LABEL, "other") == nullptr);
	CHECK_TRUE(defineService(OUTSIDER_LABEL + VFS_LABEL, SHORT_NAME) == nullptr);
	CHECK_TRUE(defineService(cock::abi::service::NO_SERVICE, "zero") == nullptr);
	CHECK_TRUE(defineService(LABEL_TOO_BIG, "big") == nullptr);

	CHECK_TRUE(findByLabel(CONSOLE_LABEL) == console);
	CHECK_TRUE(findByName(SHORT_NAME) == console);
	CHECK_TRUE(findByName(EMPTY_NAME) == nullptr);
	CHECK_TRUE(findByLabel(UNKNOWN_LABEL) == nullptr);
	CHECK_EQ(NOT_RUNNING, console->pid);
	CHECK_TRUE(findByPid(NOT_RUNNING) == nullptr);

	console->pid = SHELL_PID;
	CHECK_TRUE(findByPid(SHELL_PID) == console);
	CHECK_TRUE(findByPid(OTHER_PID) == nullptr);
}

void checkPolicy() {
	Service *vfs = defineService(VFS_LABEL, "vfs");
	Service *shell = defineService(SHELL_LABEL, "sh");
	Service *outsider = defineService(OUTSIDER_LABEL, EMPTY_NAME);
	Service *anonymous = defineService(OUTSIDER_LABEL + SHELL_LABEL, EMPTY_NAME);
	CHECK_TRUE(vfs != nullptr && shell != nullptr && outsider != nullptr && anonymous != nullptr);
	if (vfs == nullptr || shell == nullptr || outsider == nullptr || anonymous == nullptr) return;

	CHECK_FALSE(allowTalk(*shell, UNKNOWN_LABEL));
	CHECK_EQ(NO_ENTRIES, shell->allowedCount);
	CHECK_TRUE(allowTalk(*shell, VFS_LABEL));
	CHECK_TRUE(allowTalk(*shell, VFS_LABEL));
	CHECK_EQ(ONE_ENTRY, shell->allowedCount);
	CHECK_TRUE(lists(*shell, VFS_LABEL));
	CHECK_FALSE(lists(*vfs, SHELL_LABEL));

	CHECK_TRUE(mayConnect(*shell, *vfs));
	CHECK_TRUE(mayConnect(*vfs, *shell));
	CHECK_FALSE(mayConnect(*outsider, *vfs));
	CHECK_FALSE(mayConnect(*vfs, *outsider));
	CHECK_FALSE(mayConnect(*shell, *outsider));

	for (size_t index = 0; index < cock::names::MAX_ALLOWED; index++) {
		Service *extra = defineService(anonymous->label + static_cast<uint32_t>(index) + ONE_ENTRY,
									   EMPTY_NAME);
		CHECK_TRUE(extra != nullptr);
		if (extra != nullptr) allowTalk(*outsider, extra->label);
	}
	CHECK_EQ(cock::names::MAX_ALLOWED, outsider->allowedCount);
	CHECK_FALSE(allowTalk(*outsider, VFS_LABEL));
}

} // namespace

void testNames() {
	checkHeader();
	checkNames();
	checkServices();
	checkPolicy();
}

} // namespace host_test
