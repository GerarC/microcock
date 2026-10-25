#include <cock/abi/fs.hpp>
#include <cock/abi/names.hpp>
#include <cock/abi/process.hpp>
#include <cock/user/api.hpp>
#include <cock/user/exec.hpp>
#include <cock/user/fs.hpp>
#include <cock/user/names.hpp>
#include <cock/user/pm.hpp>
#include <cock/user/thread.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

namespace {

constexpr char PM_NAME[] = "pm";
constexpr char MISSING_PATH[] = "/bin/missing";
constexpr char NOT_AN_IMAGE_PATH[] = "/hello.txt";
constexpr char WORKING_DIRECTORY[] = "/bin";
constexpr char CHILD_PATH[] = "/bin/execchild";
constexpr char CHILD_NAME[] = "execchild";
constexpr char FIRST_ARGUMENT[] = "alpha";
constexpr char SECOND_ARGUMENT[] = "beta";
constexpr uint32_t ARGUMENT_COUNT = 4;
constexpr uint32_t ONE_ARGUMENT = 1;
constexpr size_t NUMBER_CHARACTERS = 11;
constexpr uint32_t DECIMAL_BASE = 10;
constexpr size_t STACK_BYTES = 0x4000;
constexpr uint32_t GATE_CLOSED = 0;
constexpr uint32_t SETTLE_MILLISECONDS = 50;
constexpr int EXIT_FAILED = 1;

alignas(cock::abi::process::PENDING_PAGE_BYTES) uint8_t sleeperStack[STACK_BYTES];
uint32_t gate = GATE_CLOSED;

void sleeper(void *) {
	for (;;) cock::user::futex_wait(&gate, GATE_CLOSED);
}

void format(uint32_t value, char (&text)[NUMBER_CHARACTERS + 1]) {
	char reversed[NUMBER_CHARACTERS];
	size_t length = 0;
	do {
		reversed[length++] = static_cast<char>('0' + value % DECIMAL_BASE);
		value /= DECIMAL_BASE;
	} while (value != 0);
	for (size_t index = 0; index < length; index++) text[index] = reversed[length - 1 - index];
	text[length] = '\0';
}

} // namespace

extern "C" int main() {
	cock::user::names::Connection manager = cock::user::names::connect(PM_NAME);
	cock::user::fs::Client files;
	if (manager.status != cock::abi::names::Status::OK || !files.connect()) {
		puts("execprobe: cannot reach pm or the file server");
		return EXIT_FAILED;
	}
	cock::user::pm::Client process(manager.handle);

	if (chdir(WORKING_DIRECTORY) != 0) {
		puts("execprobe: cannot change the directory");
		return EXIT_FAILED;
	}

	const char *none[] = {CHILD_NAME};
	bool missing = cock::user::exec(process, files, MISSING_PATH, none, ONE_ARGUMENT) ==
				   cock::user::ExecFailure::NO_FILE;
	bool notImage = cock::user::exec(process, files, NOT_AN_IMAGE_PATH, none, ONE_ARGUMENT) ==
					cock::user::ExecFailure::REFUSED;
	if (!missing || !notImage) {
		puts("execprobe: a bad exec did not fail cleanly");
		return EXIT_FAILED;
	}
	puts("execprobe: bad exec refused, image intact");

	if (cock::user::thread_start(sleeper, nullptr, sleeperStack, sizeof(sleeperStack)) == cock::user::NO_PROCESS) {
		puts("execprobe: cannot start a thread");
		return EXIT_FAILED;
	}
	cock::user::sleep_ms(SETTLE_MILLISECONDS);

	char pid[NUMBER_CHARACTERS + 1];
	format(cock::user::self(), pid);
	const char *argv[ARGUMENT_COUNT] = {CHILD_NAME, FIRST_ARGUMENT, SECOND_ARGUMENT, pid};
	cock::user::exec(process, files, CHILD_PATH, argv, ARGUMENT_COUNT);

	puts("execprobe: exec came back");
	return EXIT_FAILED;
}
