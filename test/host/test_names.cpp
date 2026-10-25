#include <cock/abi/names.hpp>
#include <names/assembly.hpp>
#include <names/forked.hpp>
#include <names/paths.hpp>
#include <names/policy.hpp>
#include <names/services.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using namespace cock::abi::names;
using cock::names::addForked;
using cock::names::addPart;
using cock::names::allowTalk;
using cock::names::Assembled;
using cock::names::defineService;
using cock::names::findByLabel;
using cock::names::findByName;
using cock::names::findByPid;
using cock::names::findClient;
using cock::names::findForked;
using cock::names::MAX_FORKED;
using cock::names::lists;
using cock::names::MAX_ASSEMBLIES;
using cock::names::MAX_PATH_BYTES;
using cock::names::mayConnect;
using cock::names::normalize;
using cock::names::NOT_RUNNING;
using cock::names::Service;

namespace {

constexpr char SHORT_NAME[] = "console";
constexpr char RUN_CONSOLE[] = "/run/console";
constexpr char THIRTEEN_BYTES[] = "abcdefghijklm";
constexpr char LONG_PATH[] = "/run/a_service_with_a_long_name";
constexpr char EMPTY_NAME[] = "";
constexpr uint32_t SOME_LABEL = 0x7ABC;
constexpr uint32_t LABEL_TOO_BIG = MAX_LABEL + 1;
constexpr uint32_t CONSOLE_LABEL = 16;
constexpr uint32_t VFS_LABEL = 17;
constexpr uint32_t SHELL_LABEL = 18;
constexpr uint32_t OUTSIDER_LABEL = 19;
constexpr uint32_t UNKNOWN_LABEL = 99;
constexpr uint32_t SHELL_PID = 7;
constexpr uint32_t OTHER_PID = 8;
constexpr uint32_t COPY_PID = 200;
constexpr uint32_t COPY_OF_COPY_PID = 201;
constexpr uint32_t STRANGER_PID = 202;
constexpr uint32_t FIRST_FILLER = 300;
constexpr uint32_t FIRST_CALLER = 100;
constexpr uint32_t SECOND_CALLER = 101;
constexpr size_t NO_ENTRIES = 0;
constexpr size_t ONE_ENTRY = 1;
constexpr size_t SECOND_PART = 1;
constexpr size_t THIRD_PART = 2;
constexpr size_t NO_PART = 0;

bool allAlive(uint32_t) { return true; }

bool noneAlive(uint32_t) { return false; }

bool onlyStrangerAlive(uint32_t pid) { return pid == STRANGER_PID; }

void send(const char *name, size_t part, size_t parts, uint32_t (&words)[CALL_WORDS]) {
	words[HEADER_WORD] = pack(Op::CONNECT, 0, part, part + 1 == parts);
	packPart(name, strlen(name), part, words);
}

void checkHeader() {
	uint32_t header = pack(Op::DEFINE, SOME_LABEL, SECOND_PART, false);
	CHECK_EQ(static_cast<uint32_t>(Op::DEFINE), opOf(header));
	CHECK_EQ(SOME_LABEL, labelOf(header));
	CHECK_EQ(SECOND_PART, partOf(header));
	CHECK_FALSE(isLast(header));

	uint32_t single = pack(Op::CONNECT);
	CHECK_EQ(static_cast<uint32_t>(Op::CONNECT), opOf(single));
	CHECK_EQ(NO_PART, partOf(single));
	CHECK_TRUE(isLast(single));
	CHECK_EQ(NO_ENTRIES, static_cast<size_t>(labelOf(single)));
	CHECK_EQ(LABEL_TOO_BIG & MAX_LABEL, labelOf(pack(Op::BIND, LABEL_TOO_BIG)));
}

void checkParts() {
	CHECK_EQ(NO_ENTRIES, partCount(0));
	CHECK_EQ(ONE_ENTRY, partCount(PART_BYTES));
	CHECK_EQ(SECOND_PART + ONE_ENTRY, partCount(PART_BYTES + ONE_ENTRY));
	CHECK_EQ(MAX_PARTS, partCount(MAX_NAME_BYTES));

	uint32_t words[CALL_WORDS] = {};
	char bytes[PART_BYTES] = {};
	CHECK_TRUE(packPart(SHORT_NAME, strlen(SHORT_NAME), NO_PART, words));
	CHECK_EQ(strlen(SHORT_NAME), unpackPart(words, bytes));
	CHECK_TRUE(memcmp(bytes, SHORT_NAME, strlen(SHORT_NAME)) == 0);

	CHECK_TRUE(packPart(THIRTEEN_BYTES, strlen(THIRTEEN_BYTES), NO_PART, words));
	CHECK_EQ(PART_BYTES, unpackPart(words, bytes));
	CHECK_TRUE(packPart(THIRTEEN_BYTES, strlen(THIRTEEN_BYTES), SECOND_PART, words));
	CHECK_EQ(ONE_ENTRY, unpackPart(words, bytes));
	CHECK_EQ(static_cast<int>(THIRTEEN_BYTES[PART_BYTES]), static_cast<int>(bytes[NO_PART]));

	CHECK_FALSE(packPart(THIRTEEN_BYTES, strlen(THIRTEEN_BYTES), THIRD_PART, words));
	CHECK_EQ(NO_ENTRIES, unpackPart(words, bytes));
	CHECK_FALSE(packPart(EMPTY_NAME, NO_ENTRIES, NO_PART, words));
	CHECK_FALSE(packPart(THIRTEEN_BYTES, MAX_NAME_BYTES + ONE_ENTRY, NO_PART, words));
}

void checkAssembly() {
	char name[MAX_NAME_BYTES + 1] = {};
	uint32_t words[CALL_WORDS] = {};
	size_t parts = partCount(strlen(LONG_PATH));

	for (size_t part = 0; part + ONE_ENTRY < parts; part++) {
		send(LONG_PATH, part, parts, words);
		CHECK_TRUE(addPart(FIRST_CALLER, words[HEADER_WORD], words, allAlive, false, name) ==
				   Assembled::MORE);
	}
	send(LONG_PATH, parts - ONE_ENTRY, parts, words);
	CHECK_TRUE(addPart(FIRST_CALLER, words[HEADER_WORD], words, allAlive, false, name) ==
			   Assembled::DONE);
	CHECK_TRUE(strcmp(name, LONG_PATH) == 0);

	send(SHORT_NAME, NO_PART, ONE_ENTRY, words);
	CHECK_TRUE(addPart(FIRST_CALLER, words[HEADER_WORD], words, allAlive, false, name) ==
			   Assembled::DONE);
	CHECK_TRUE(strcmp(name, SHORT_NAME) == 0);

	send(LONG_PATH, SECOND_PART, parts, words);
	CHECK_TRUE(addPart(FIRST_CALLER, words[HEADER_WORD], words, allAlive, false, name) ==
			   Assembled::BAD);

	send(LONG_PATH, NO_PART, parts, words);
	CHECK_TRUE(addPart(FIRST_CALLER, words[HEADER_WORD], words, allAlive, false, name) ==
			   Assembled::MORE);
	send(LONG_PATH, THIRD_PART, parts, words);
	CHECK_TRUE(addPart(FIRST_CALLER, words[HEADER_WORD], words, allAlive, false, name) ==
			   Assembled::BAD);

	words[HEADER_WORD] = pack(Op::CONNECT);
	packPart(SHORT_NAME, NO_ENTRIES, NO_PART, words);
	CHECK_TRUE(addPart(FIRST_CALLER, words[HEADER_WORD], words, allAlive, false, name) ==
			   Assembled::BAD);
	CHECK_TRUE(addPart(FIRST_CALLER, words[HEADER_WORD], words, allAlive, true, name) ==
			   Assembled::DONE);
	CHECK_EQ(NO_ENTRIES, strlen(name));

	send(THIRTEEN_BYTES, NO_PART, parts + ONE_ENTRY, words);
	words[HEADER_WORD] = pack(Op::CONNECT, 0, NO_PART, false);
	words[FIRST_NAME_WORD + ONE_ENTRY] = 0;
	CHECK_TRUE(addPart(FIRST_CALLER, words[HEADER_WORD], words, allAlive, false, name) ==
			   Assembled::BAD);
}

void checkAssemblySlots() {
	char name[MAX_NAME_BYTES + 1] = {};
	uint32_t words[CALL_WORDS] = {};
	size_t parts = partCount(strlen(LONG_PATH));

	for (uint32_t caller = 0; caller < MAX_ASSEMBLIES; caller++) {
		send(LONG_PATH, NO_PART, parts, words);
		CHECK_TRUE(addPart(caller + SECOND_CALLER + MAX_ASSEMBLIES, words[HEADER_WORD], words,
						   allAlive, false, name) == Assembled::MORE);
	}
	send(LONG_PATH, NO_PART, parts, words);
	CHECK_TRUE(addPart(SECOND_CALLER, words[HEADER_WORD], words, allAlive, false, name) ==
			   Assembled::BAD);
	CHECK_TRUE(addPart(SECOND_CALLER, words[HEADER_WORD], words, noneAlive, false, name) ==
			   Assembled::MORE);
}

void checkPaths() {
	char path[MAX_PATH_BYTES + 1] = {};
	normalize(SHORT_NAME, path);
	CHECK_TRUE(strcmp(path, RUN_CONSOLE) == 0);
	normalize(RUN_CONSOLE, path);
	CHECK_TRUE(strcmp(path, RUN_CONSOLE) == 0);
	normalize(EMPTY_NAME, path);
	CHECK_EQ(NO_ENTRIES, strlen(path));

	char longest[MAX_NAME_BYTES + 1];
	memset(longest, 'x', MAX_NAME_BYTES);
	longest[MAX_NAME_BYTES] = '\0';
	normalize(longest, path);
	CHECK_EQ(MAX_PATH_BYTES, strlen(path));
	CHECK_TRUE(memcmp(path, RUN_PREFIX, RUN_PREFIX_BYTES) == 0);
}

void checkServices() {
	Service *console = defineService(CONSOLE_LABEL, RUN_CONSOLE);
	CHECK_TRUE(console != nullptr);
	CHECK_TRUE(defineService(CONSOLE_LABEL, "/run/other") == nullptr);
	CHECK_TRUE(defineService(OUTSIDER_LABEL + VFS_LABEL, RUN_CONSOLE) == nullptr);
	CHECK_TRUE(defineService(cock::abi::service::NO_SERVICE, "/run/zero") == nullptr);
	CHECK_TRUE(defineService(LABEL_TOO_BIG, "/run/big") == nullptr);

	CHECK_TRUE(findByLabel(CONSOLE_LABEL) == console);
	CHECK_TRUE(findByName(RUN_CONSOLE) == console);
	CHECK_TRUE(findByName(EMPTY_NAME) == nullptr);
	CHECK_TRUE(findByLabel(UNKNOWN_LABEL) == nullptr);
	CHECK_EQ(NOT_RUNNING, console->pid);
	CHECK_TRUE(findByPid(NOT_RUNNING) == nullptr);

	console->pid = SHELL_PID;
	CHECK_TRUE(findByPid(SHELL_PID) == console);
	CHECK_TRUE(findByPid(OTHER_PID) == nullptr);
}

void checkPolicy() {
	Service *vfs = defineService(VFS_LABEL, "/run/vfs");
	Service *shell = defineService(SHELL_LABEL, "/run/sh");
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

void checkForked() {
	Service *console = findByLabel(CONSOLE_LABEL);
	CHECK_TRUE(console != nullptr);

	CHECK_TRUE(addForked(COPY_PID, SHELL_PID, allAlive));
	CHECK_TRUE(findForked(COPY_PID, allAlive) == console);
	CHECK_TRUE(findClient(COPY_PID, allAlive) == console);
	CHECK_TRUE(findClient(SHELL_PID, allAlive) == console);
	CHECK_TRUE(findForked(COPY_PID, noneAlive) == nullptr);
	CHECK_TRUE(findForked(SHELL_PID, allAlive) == nullptr);
	CHECK_EQ(SHELL_PID, console->pid);

	CHECK_TRUE(addForked(COPY_PID, SHELL_PID, allAlive));
	CHECK_TRUE(addForked(COPY_OF_COPY_PID, COPY_PID, allAlive));
	CHECK_TRUE(findClient(COPY_OF_COPY_PID, allAlive) == console);

	CHECK_FALSE(addForked(STRANGER_PID, OTHER_PID, allAlive));
	CHECK_FALSE(addForked(STRANGER_PID, STRANGER_PID, allAlive));
	CHECK_FALSE(addForked(SHELL_PID, COPY_PID, allAlive));
	CHECK_FALSE(addForked(STRANGER_PID, SHELL_PID, noneAlive));
	CHECK_FALSE(addForked(NOT_RUNNING, SHELL_PID, allAlive));
	CHECK_TRUE(findClient(STRANGER_PID, allAlive) == nullptr);

	for (uint32_t index = 0; index < MAX_FORKED; index++) addForked(FIRST_FILLER + index, SHELL_PID, allAlive);
	CHECK_FALSE(addForked(STRANGER_PID, SHELL_PID, allAlive));
	CHECK_TRUE(addForked(STRANGER_PID, SHELL_PID, onlyStrangerAlive));
	CHECK_TRUE(findForked(STRANGER_PID, onlyStrangerAlive) == console);
}

} // namespace

void testNames() {
	checkHeader();
	checkParts();
	checkAssembly();
	checkAssemblySlots();
	checkPaths();
	checkServices();
	checkPolicy();
	checkForked();
}

} // namespace host_test
