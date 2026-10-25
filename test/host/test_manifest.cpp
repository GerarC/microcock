#include <cock/abi/manifest.hpp>
#include <test/host/check.hpp>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace host_test {

using cock::abi::CapabilityType;
using cock::abi::manifest::Entry;
using cock::abi::manifest::Error;
using cock::abi::manifest::Manifest;

namespace {

constexpr char FULL_MANIFEST[] =
	"vga_driver caps=mem:0xA0000+0x20000\n"
	"fsclient path=bin/fsclient.elf repeat=10\n"
	"\n"
	"# a comment\n"
	"   kbd path=bin/kbd.elf caps=io:0x60+1,irq:1\r\n"
	"probe path=bin/probe.elf";
constexpr size_t FULL_ENTRY_COUNT = 4;
constexpr size_t VGA_INDEX = 0;
constexpr size_t FSCLIENT_INDEX = 1;
constexpr size_t KBD_INDEX = 2;
constexpr size_t PROBE_INDEX = 3;
constexpr uint32_t VIDEO_START = 0xA0000;
constexpr uint32_t VIDEO_SIZE = 0x20000;
constexpr uint32_t PS2_PORT = 0x60;
constexpr uint32_t KEYBOARD_LINE = 1;
constexpr uint32_t FSCLIENT_RUNS = 10;
constexpr uint32_t RESTART_LIMIT = 3;
constexpr size_t KBD_GRANTS = 2;
constexpr size_t KBD_PORT_GRANT = 0;
constexpr size_t KBD_IRQ_GRANT = 1;
constexpr size_t SECOND_LINE = 2;
constexpr size_t THIRD_LINE = 3;
constexpr size_t FOURTH_LINE = 4;
constexpr size_t NO_ENTRIES = 0;
constexpr size_t NO_GRANTS = 0;
constexpr size_t NO_LINE = 0;
constexpr size_t ONE_ENTRY = 1;
constexpr size_t ONE_GRANT = 1;
constexpr uint32_t SINGLE_UNIT = 1;
constexpr size_t PORT_AND_SPAWN_GRANTS = 2;
constexpr size_t SPAWN_GRANT_INDEX = 1;
constexpr uint32_t PAIR_UNITS = 2;
constexpr size_t DELEGATE_GRANTS = 3;
constexpr size_t FIRST_GRANT = 0;
constexpr size_t SECOND_GRANT = 1;
constexpr size_t THIRD_GRANT = 2;
constexpr uint32_t SERIAL_SECOND_PORT = 0x2F8;
constexpr uint32_t HEX_FF_VALUE = 0xFF;
constexpr size_t NAME_BUFFER_BYTES = 128;
constexpr size_t LONG_TEXT_BYTES = 256;
constexpr char LONG_FILL = 'a';
constexpr char REPEAT_BASE[] = "tool path=bin/tool.elf repeat=";

constexpr size_t NO_NAMES = 0;
constexpr size_t ONE_NAME = 1;
constexpr size_t TWO_NAMES = 2;
constexpr char FIRST_PEER_LETTER = 'a';

struct Text {
	char bytes[LONG_TEXT_BYTES];

	const char *c_str() const { return bytes; }
};

Text longIpcName() {
	Text text = {};
	size_t length = 0;
	const char *prefix = "a ipc=";
	for (; prefix[length] != '\0'; length++)
		text.bytes[length] = prefix[length];
	for (size_t index = 0; index < cock::abi::manifest::MAX_NAME_BYTES; index++)
		text.bytes[length++] = LONG_FILL;
	text.bytes[length] = '\0';
	return text;
}

Text ipcListOf(size_t names) {
	Text text = {};
	size_t length = 0;
	const char *prefix = "a ipc=";
	for (; prefix[length] != '\0'; length++)
		text.bytes[length] = prefix[length];
	for (size_t index = 0; index < names; index++) {
		if (index != 0) text.bytes[length++] = ',';
		text.bytes[length++] = static_cast<char>(FIRST_PEER_LETTER + index);
	}
	text.bytes[length] = '\0';
	return text;
}

bool parses(const char *text, Manifest &manifest) {
	return manifest.parse(text, strlen(text));
}

Error errorOf(const char *text) {
	Manifest manifest;
	if (parses(text, manifest)) return Error::NONE;
	return manifest.error;
}

void checkFullManifest() {
	Manifest manifest;
	CHECK_TRUE(parses(FULL_MANIFEST, manifest));
	CHECK_EQ(FULL_ENTRY_COUNT, manifest.count);

	const Entry &vga = manifest.entries[VGA_INDEX];
	CHECK_TRUE(strcmp(vga.name, "vga_driver") == 0);
	CHECK_TRUE(vga.isBootModule());
	CHECK_EQ(ONE_GRANT, vga.grantCount);
	CHECK_TRUE(vga.grants[0].type == CapabilityType::PHYSICAL_MEMORY);
	CHECK_EQ(VIDEO_START, vga.grants[0].first);
	CHECK_EQ(VIDEO_SIZE, vga.grants[0].count);
	CHECK_EQ(cock::abi::manifest::DEFAULT_REPEAT, vga.repeat);

	const Entry &client = manifest.entries[FSCLIENT_INDEX];
	CHECK_FALSE(client.isBootModule());
	CHECK_TRUE(strcmp(client.path, "bin/fsclient.elf") == 0);
	CHECK_EQ(FSCLIENT_RUNS, client.repeat);
	CHECK_EQ(NO_GRANTS, client.grantCount);

	const Entry &keyboard = manifest.entries[KBD_INDEX];
	CHECK_EQ(KBD_GRANTS, keyboard.grantCount);
	CHECK_TRUE(keyboard.grants[KBD_PORT_GRANT].type == CapabilityType::IO_PORTS);
	CHECK_EQ(PS2_PORT, keyboard.grants[KBD_PORT_GRANT].first);
	CHECK_EQ(SINGLE_UNIT, keyboard.grants[KBD_PORT_GRANT].count);
	CHECK_TRUE(keyboard.grants[KBD_IRQ_GRANT].type == CapabilityType::IRQ);
	CHECK_EQ(KEYBOARD_LINE, keyboard.grants[KBD_IRQ_GRANT].first);
	CHECK_EQ(SINGLE_UNIT, keyboard.grants[KBD_IRQ_GRANT].count);

	CHECK_TRUE(strcmp(manifest.entries[PROBE_INDEX].name, "probe") == 0);
	CHECK_TRUE(manifest.find("kbd") == &manifest.entries[KBD_INDEX]);
	CHECK_TRUE(manifest.find("missing") == nullptr);
	CHECK_TRUE(manifest.find("kb") == nullptr);
}

void checkEmptyInput() {
	Manifest manifest;
	CHECK_TRUE(manifest.parse("", 0));
	CHECK_EQ(NO_ENTRIES, manifest.count);
	CHECK_TRUE(parses("\n\n   \n", manifest));
	CHECK_EQ(NO_ENTRIES, manifest.count);
	CHECK_TRUE(parses("# only a comment\n# another\n", manifest));
	CHECK_EQ(NO_ENTRIES, manifest.count);
}

void checkNumbers() {
	Manifest manifest;
	CHECK_TRUE(parses("a caps=io:96+2\n", manifest));
	CHECK_EQ(PS2_PORT, manifest.entries[0].grants[0].first);
	CHECK_EQ(PAIR_UNITS, manifest.entries[0].grants[0].count);
	CHECK_TRUE(parses("a caps=io:0XfF+1\n", manifest));
	CHECK_EQ(HEX_FF_VALUE, manifest.entries[0].grants[0].first);
	CHECK_TRUE(parses("a caps=mem:0xFFFFF000+0x1000\n", manifest));
	CHECK_FALSE(parses("a caps=mem:0xFFFFF000+0x2000\n", manifest));
	CHECK_FALSE(parses("a caps=io:4294967296+1\n", manifest));
	CHECK_FALSE(parses("a caps=io:0x+1\n", manifest));
	CHECK_FALSE(parses("a caps=io:12z+1\n", manifest));
	CHECK_FALSE(parses("a caps=io:-1\n", manifest));
	CHECK_FALSE(parses("a caps=io:1+0\n", manifest));
	CHECK_FALSE(parses("a caps=io:+1\n", manifest));
	CHECK_FALSE(parses("a caps=mem:0xA0000\n", manifest));
}

void checkGrantRules() {
	Manifest manifest;
	CHECK_TRUE(parses("a caps=irq:4\n", manifest));
	CHECK_EQ(SINGLE_UNIT, manifest.entries[0].grants[0].count);
	CHECK_TRUE(parses("a caps=io:1,io:2,io:3,io:4\n", manifest));
	CHECK_EQ(cock::abi::manifest::MAX_GRANTS, manifest.entries[0].grantCount);
	CHECK_TRUE(errorOf("a caps=io:1,io:2,io:3,io:4,io:5\n") == Error::TOO_MANY_GRANTS);
	CHECK_TRUE(errorOf("a caps=disk:1\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("a caps=io\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("a caps=io:1,,irq:1\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("a caps=io:1 caps=irq:1\n") == Error::DUPLICATE_KEY);
}

void checkSpawnGrant() {
	Manifest manifest;
	CHECK_TRUE(parses("a caps=io:1,spawn\n", manifest));
	CHECK_EQ(PORT_AND_SPAWN_GRANTS, manifest.entries[0].grantCount);
	const cock::abi::manifest::Grant &spawn = manifest.entries[0].grants[SPAWN_GRANT_INDEX];
	CHECK_TRUE(spawn.type == cock::abi::CapabilityType::SPAWN);
	CHECK_EQ(cock::abi::SPAWN_FIRST, spawn.first);
	CHECK_EQ(cock::abi::SPAWN_COUNT, spawn.count);
	CHECK_TRUE(errorOf("a caps=spawn:1\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("a caps=spawns\n") == Error::BAD_VALUE);
}

void checkDelegateMark() {
	Manifest manifest;
	CHECK_TRUE(parses("a caps=io:0x2F8+2*,irq:5,spawn*\n", manifest));
	CHECK_EQ(DELEGATE_GRANTS, manifest.entries[0].grantCount);
	const cock::abi::manifest::Grant *grants = manifest.entries[0].grants;
	CHECK_TRUE(grants[FIRST_GRANT].delegate);
	CHECK_EQ(SERIAL_SECOND_PORT, grants[FIRST_GRANT].first);
	CHECK_EQ(PAIR_UNITS, grants[FIRST_GRANT].count);
	CHECK_FALSE(grants[SECOND_GRANT].delegate);
	CHECK_TRUE(grants[THIRD_GRANT].delegate);
	CHECK_TRUE(grants[THIRD_GRANT].type == cock::abi::CapabilityType::SPAWN);
	CHECK_TRUE(errorOf("a caps=*\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("a caps=io:1**\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("a caps=io:*\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("a caps=*io:1\n") == Error::BAD_VALUE);
}

void checkKeyRules() {
	CHECK_TRUE(errorOf("a flag\n") == Error::UNKNOWN_KEY);
	CHECK_TRUE(errorOf("a mode=fast\n") == Error::UNKNOWN_KEY);
	CHECK_TRUE(errorOf("a path=x path=y\n") == Error::DUPLICATE_KEY);
	CHECK_TRUE(errorOf("a path=x repeat=2 repeat=3\n") == Error::DUPLICATE_KEY);
	CHECK_TRUE(errorOf("a path=\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("a peer=b\n") == Error::UNKNOWN_KEY);
}

void checkRepeatBounds() {
	char text[NAME_BUFFER_BYTES];
	const size_t base = strlen(REPEAT_BASE);
	memcpy(text, REPEAT_BASE, base);

	text[base] = '\0';
	strcat(text, "0\n");
	CHECK_TRUE(errorOf(text) == Error::BAD_VALUE);

	text[base] = '\0';
	strcat(text, "32\n");
	CHECK_TRUE(errorOf(text) == Error::NONE);

	text[base] = '\0';
	strcat(text, "33\n");
	CHECK_TRUE(errorOf(text) == Error::BAD_VALUE);

	CHECK_TRUE(errorOf("tool repeat=2\n") == Error::BAD_VALUE);
}

void checkRestartRules() {
	Manifest manifest;
	CHECK_TRUE(parses("server path=bin/server.elf restart=3\n", manifest));
	CHECK_EQ(RESTART_LIMIT, manifest.entries[0].restarts);
	CHECK_TRUE(parses("server path=bin/server.elf\n", manifest));
	CHECK_EQ(cock::abi::manifest::NO_RESTARTS, manifest.entries[0].restarts);
	CHECK_TRUE(parses("server path=x restart=16\n", manifest));
	CHECK_TRUE(errorOf("server path=x restart=17\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("server path=x restart=0\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("server path=x restart=\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("server path=x restart=1 restart=2\n") == Error::DUPLICATE_KEY);
	CHECK_TRUE(errorOf("server restart=1\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("server path=x repeat=2 restart=1\n") == Error::BAD_VALUE);
}

void checkParentRules() {
	Manifest manifest;
	CHECK_TRUE(parses("shell path=bin/sh.elf parent=init\n", manifest));
	CHECK_TRUE(manifest.entries[0].startedByInit);
	CHECK_TRUE(parses("shell path=bin/sh.elf\n", manifest));
	CHECK_FALSE(manifest.entries[0].startedByInit);
	CHECK_TRUE(errorOf("shell path=x parent=rs\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("shell path=x parent=\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("shell path=x parent=init parent=init\n") == Error::DUPLICATE_KEY);
	CHECK_TRUE(errorOf("module parent=init\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("shell path=x repeat=2 parent=init\n") == Error::BAD_VALUE);
	CHECK_TRUE(errorOf("shell path=x restart=2 parent=init\n") == Error::BAD_VALUE);
}

void checkIpcRules() {
	Manifest manifest;
	CHECK_TRUE(parses("sh path=bin/sh.elf ipc=console,vfs\n", manifest));
	const Entry &shell = manifest.entries[0];
	CHECK_EQ(TWO_NAMES, shell.ipcCount);
	CHECK_TRUE(strcmp(shell.ipc[0], "console") == 0);
	CHECK_TRUE(strcmp(shell.ipc[1], "vfs") == 0);

	CHECK_TRUE(parses("a path=bin/a.elf\n", manifest));
	CHECK_EQ(NO_NAMES, manifest.entries[0].ipcCount);
	CHECK_TRUE(parses("a path=bin/a.elf ipc=b caps=io:1\n", manifest));
	CHECK_EQ(ONE_NAME, manifest.entries[0].ipcCount);
	CHECK_TRUE(Error::BAD_VALUE == errorOf("a ipc=\n"));
	CHECK_TRUE(Error::BAD_VALUE == errorOf("a ipc=b,\n"));
	CHECK_TRUE(Error::BAD_VALUE == errorOf("a ipc=,b\n"));
	CHECK_TRUE(Error::BAD_VALUE == errorOf("a ipc=b,b\n"));
	CHECK_TRUE(Error::DUPLICATE_KEY == errorOf("a ipc=b ipc=c\n"));
	CHECK_TRUE(Error::BAD_VALUE == errorOf(longIpcName().c_str()));
	CHECK_TRUE(Error::NONE == errorOf(ipcListOf(cock::abi::manifest::MAX_IPC_PEERS).c_str()));
	CHECK_TRUE(Error::TOO_MANY_PEERS ==
			   errorOf(ipcListOf(cock::abi::manifest::MAX_IPC_PEERS + ONE_NAME).c_str()));
}

void checkNames() {
	char text[LONG_TEXT_BYTES];
	memset(text, LONG_FILL, sizeof(text) - 1);
	text[sizeof(text) - 1] = '\0';
	CHECK_TRUE(errorOf(text) == Error::BAD_NAME);

	CHECK_TRUE(errorOf("path=bin/x\n") == Error::BAD_NAME);
	CHECK_TRUE(errorOf("a\nb\na\n") == Error::DUPLICATE_NAME);
	CHECK_TRUE(errorOf("a\nb\nc\n") == Error::NONE);
}

void checkErrorLines() {
	Manifest manifest;
	CHECK_FALSE(parses("a\n\nb flag\n", manifest));
	CHECK_EQ(THIRD_LINE, manifest.errorLine);
	CHECK_FALSE(parses("a\nb path=x path=y\n", manifest));
	CHECK_EQ(SECOND_LINE, manifest.errorLine);
	CHECK_FALSE(parses("\n# c\n\nx y\n", manifest));
	CHECK_EQ(FOURTH_LINE, manifest.errorLine);
}

void checkEntryLimit() {
	char text[LONG_TEXT_BYTES];
	size_t length = 0;
	for (size_t entry = 0; entry < cock::abi::manifest::MAX_ENTRIES; entry++) {
		text[length++] = LONG_FILL;
		text[length++] = static_cast<char>(LONG_FILL + 1 + entry);
		text[length++] = '\n';
	}

	Manifest manifest;
	CHECK_TRUE(manifest.parse(text, length));
	CHECK_EQ(cock::abi::manifest::MAX_ENTRIES, manifest.count);

	text[length++] = LONG_FILL;
	text[length++] = LONG_FILL;
	text[length++] = '\n';
	CHECK_FALSE(manifest.parse(text, length));
	CHECK_TRUE(manifest.error == Error::TOO_MANY_ENTRIES);
}

void checkRejectionKeepsNothing() {
	Manifest manifest;
	CHECK_FALSE(parses("good path=x\nbad flag\n", manifest));
	CHECK_EQ(ONE_ENTRY, manifest.count);
	CHECK_TRUE(manifest.error == Error::UNKNOWN_KEY);
	CHECK_TRUE(parses("good path=x\n", manifest));
	CHECK_TRUE(manifest.error == Error::NONE);
	CHECK_EQ(NO_LINE, manifest.errorLine);
}

} // namespace

void testManifest() {
	checkFullManifest();
	checkEmptyInput();
	checkNumbers();
	checkGrantRules();
	checkSpawnGrant();
	checkDelegateMark();
	checkKeyRules();
	checkRepeatBounds();
	checkRestartRules();
	checkParentRules();
	checkIpcRules();
	checkNames();
	checkErrorLines();
	checkEntryLimit();
	checkRejectionKeepsNothing();
}

} // namespace host_test
