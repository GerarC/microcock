#include <cock/abi/names.hpp>
#include <cock/abi/pm.hpp>
#include <cock/user/api.hpp>
#include <cock/user/names.hpp>
#include <cock/user/pm.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

using cock::abi::pm::Status;
using cock::user::pm::Forked;
using cock::user::pm::Waited;

namespace {

constexpr char SERVER_NAME[] = "pm";
constexpr char ALLOWED_NAME[] = "console";
constexpr char FORBIDDEN_NAME[] = "vfs";
constexpr uint32_t ORIGINAL = 0x600D;
constexpr uint32_t PARENT_CHANGED = 0xA11CE;
constexpr uint32_t CHILD_CHANGED = 0xB0B;
constexpr int PASSED = 0;
constexpr int FAILED = 1;
constexpr int CHILD_SAW_WRONG_VALUES = 2;
constexpr int CHILD_COULD_NOT_FORK = 3;
constexpr int CHILD_LOST_ITS_VALUES = 4;
constexpr int CHILD_BAD_WAIT = 5;
constexpr int GRANDCHILD_SAW_WRONG_VALUES = 6;
constexpr int CHILD_NAMES_REFUSED = 7;
constexpr int CHILD_NAMES_TOO_OPEN = 8;
constexpr int GRANDCHILD_NAMES_WRONG = 9;
constexpr int CHILD_PASSED = 42;
constexpr int GRANDCHILD_PASSED = 43;

volatile uint32_t dataValue = ORIGINAL;

bool connects(const char *name, cock::abi::names::Status expected) {
	return cock::user::names::connect(name).status == expected;
}

int runGrandchild(volatile uint32_t &local) {
	bool inherited = dataValue == CHILD_CHANGED && local == CHILD_CHANGED;
	if (!inherited) return GRANDCHILD_SAW_WRONG_VALUES;
	if (!connects(ALLOWED_NAME, cock::abi::names::Status::OK) ||
		!connects(FORBIDDEN_NAME, cock::abi::names::Status::DENIED))
		return GRANDCHILD_NAMES_WRONG;
	return GRANDCHILD_PASSED;
}

int runChild(cock::user::pm::Client &client, volatile uint32_t &local) {
	if (dataValue != ORIGINAL || local != ORIGINAL) return CHILD_SAW_WRONG_VALUES;
	dataValue = CHILD_CHANGED;
	local = CHILD_CHANGED;
	if (!connects(ALLOWED_NAME, cock::abi::names::Status::OK)) return CHILD_NAMES_REFUSED;
	if (!connects(FORBIDDEN_NAME, cock::abi::names::Status::DENIED)) return CHILD_NAMES_TOO_OPEN;

	Forked grandchild = client.fork();
	if (grandchild.status != Status::OK) return CHILD_COULD_NOT_FORK;
	if (grandchild.isChild()) return runGrandchild(local);

	Waited waited = client.wait(grandchild.pid);
	bool collected = waited.status == Status::OK && waited.pid == grandchild.pid &&
					 waited.exitStatus == static_cast<uint32_t>(GRANDCHILD_PASSED);
	if (!collected) return CHILD_BAD_WAIT;
	return dataValue == CHILD_CHANGED && local == CHILD_CHANGED ? CHILD_PASSED : CHILD_LOST_ITS_VALUES;
}

int runParent(cock::user::pm::Client &client, uint32_t child, volatile uint32_t &local) {
	dataValue = PARENT_CHANGED;
	local = PARENT_CHANGED;

	Waited waited = client.wait(child);
	bool collected = waited.status == Status::OK && waited.pid == child &&
					 waited.exitStatus == static_cast<uint32_t>(CHILD_PASSED);
	bool intact = dataValue == PARENT_CHANGED && local == PARENT_CHANGED;
	if (collected && intact) {
		puts("forkprobe: ok");
		return PASSED;
	}
	printf("forkprobe: wrong: status %u, child %u, collected %u, exit %u, data %x, stack %x\n",
		   static_cast<unsigned>(waited.status), static_cast<unsigned>(child),
		   static_cast<unsigned>(waited.pid), static_cast<unsigned>(waited.exitStatus),
		   static_cast<unsigned>(dataValue), static_cast<unsigned>(local));
	return FAILED;
}

} // namespace

extern "C" int main() {
	cock::user::names::Connection manager = cock::user::names::connect(SERVER_NAME);
	if (manager.status != cock::abi::names::Status::OK) {
		puts("forkprobe: cannot reach pm");
		return FAILED;
	}

	cock::user::pm::Client client(manager.handle);
	volatile uint32_t local = ORIGINAL;
	Forked forked = client.fork();
	if (forked.status != Status::OK) {
		puts("forkprobe: fork failed");
		return FAILED;
	}
	if (forked.isChild()) return runChild(client, local);
	return runParent(client, forked.pid, local);
}
